"""Generate a host conformance test from the resolved M5GFX raster bodies.

The component stays unchanged. CMake writes the generated test into its build
folder; upstream signature changes deliberately require review of this seam.
"""
import argparse
import hashlib
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('vendor_source', type=Path)
parser.add_argument('output', type=Path)
args = parser.parse_args()
vendor = args.vendor_source
source = vendor.read_text(encoding='utf-8')
def body(signature):
    begin = source.index(signature)
    opening = source.index('{', begin)
    depth = 1
    pos = opening + 1
    while depth:
        depth += (source[pos] == '{') - (source[pos] == '}')
        pos += 1
    return source[begin:pos]

rotation = body('void LGFXBase::make_rotation_matrix(').replace('LGFXBase::', '')
inverse = body('static bool make_invert_affine32(')
raster = body('void LGFXBase::push_image_affine(const float* matrix, pixelcopy_t* pc)').replace('LGFXBase::', 'Raster::')
harness = r'''
#include "map_preview.hpp"
#include "map_scaled_preview.hpp"
#include "lgfx/v1/misc/pixelcopy.hpp"
#include <vector>
#include <array>
#include <cassert>
#include <cstdio>
using lgfx::pixelcopy_t;
constexpr unsigned FP_SCALE=16;
constexpr float deg_to_rad=0.017453292519943295f;
struct Target {
  static constexpr int width=1280,height=720;
  int32_t x=0,y=0,w=width,h=height;
  std::vector<unsigned char> cleared=std::vector<unsigned char>(width*height);
  std::vector<unsigned char> covered=std::vector<unsigned char>(width*height);
  std::vector<uint16_t> pixels=std::vector<uint16_t>(width*height,0xdead);
  void getClipRect(int32_t* a,int32_t* b,int32_t* c,int32_t* d){*a=x;*b=y;*c=w;*d=h;}
  void setClipRect(int32_t a,int32_t b,int32_t c,int32_t d){x=a;y=b;w=c;h=d;}
  void fillRect(int l,int t,int rw,int rh,int) {
    assert(l>=x&&t>=y&&l+rw<=x+w&&t+rh<=y+h);
    for(int j=t;j<t+rh;++j)for(int i=l;i<l+rw;++i){++cleared[j*width+i];pixels[j*width+i]=0x0841;}
  }
};
struct Panel {
  Target* target;
  void writeImage(int x,int y,int w,int h,pixelcopy_t* pc,bool) {
    assert(h==1&&pc->transp==pixelcopy_t::NON_TRANSP);
    assert(y>=target->y&&y<target->y+target->h&&x>=target->x&&x+w<=target->x+target->w);
    for(int i=0;i<w;++i) {
      assert((pc->src_x32>>16)<unsigned(pc->src_width));
      assert((pc->src_y32>>16)<unsigned(pc->src_height));
      ++target->covered[y*Target::width+x+i];
      target->pixels[y*Target::width+x+i]=static_cast<const uint16_t*>(pc->src_data)[pc->src_y*pc->src_width+pc->src_x];
      pc->src_x32+=pc->src_x32_add;pc->src_y32+=pc->src_y32_add;
    }
  }
};
struct Raster {
  int32_t _clip_l,_clip_t,_clip_r,_clip_b;
  Panel* _panel;
  void startWrite(){} void endWrite(){}
  void push_image_affine(const float*,pixelcopy_t*);
};
'''
harness += rotation+'\n'+inverse+'\n'+raster+'\n'
harness += r'''
struct Image {
  int w,h;
  std::vector<uint16_t> pixels;
  Image(int width,int height):w(width),h(height),pixels(w*h) {
    for(int j=0;j<h;++j)for(int i=0;i<w;++i)pixels[j*w+i]=uint16_t((i*997)^(j*3137));
  }
  void setPivot(int x,int y){assert(x==0&&y==0);}
  void pushSprite(Target*,int,int){assert(false);}
  void pushRotateZoom(Target* target,float dx,float dy,int angle,float sx,float sy) {
    assert(angle==0);
    float matrix[6];make_rotation_matrix(matrix,dx+.5f,dy+.5f,.5f,.5f,0,sx,sy);
    pixelcopy_t pc;pc.src_width=w;pc.src_height=h;pc.src_data=pixels.data();
    Panel panel{target};
    Raster raster{target->x,target->y,target->x+target->w-1,target->y+target->h-1,&panel};
    raster.push_image_affine(matrix,&pc);
  }
};
int main() {
  unsigned cases=0;
  std::array<uint16_t,1280*16+2> stripe{};
  for(int layout=0;layout<4;++layout) {
    const int widths[]={1248,842,622,287},heights[]={568,330,302,153};
    const int w=widths[layout],h=heights[layout],x=16,y=80;
    for(double scale:{.25,.5,1.25,2.,4.,std::nextafter(2.,0.)})
      for(double dx:{-1400.,-123.75,0.,.25,125.,1400.})
        for(double dy:{-720.,-11.25,0.,.75,720.})for(int clipped=0;clipped<2;++clipped) {
          Target target;Image image{w,h};
          const int cx=clipped?x+13:0,cy=clipped?y+17:0;
          const int cw=clipped?w/2:1280,ch=clipped?h/2:720;
          target.setClipRect(cx,cy,cw,ch);
          orcsdr::maps::paint_preview(image,target,{scale,dx,dy},x,y,w,h);
          assert(target.x==cx&&target.y==cy&&target.w==cw&&target.h==ch);
          unsigned retained=0;
          for(int j=y;j<y+h;++j)for(int i=x;i<x+w;++i) {
            const auto k=j*1280+i;
            if(i<cx||i>=cx+cw||j<cy||j>=cy+ch) {
              assert(!target.cleared[k]&&!target.covered[k]);continue;
            }
            if(!target.cleared[k]&&!target.covered[k]) {
              std::fprintf(stderr,"STALE scale=%g dx=%g dy=%g at %d,%d\n",scale,dx,dy,i,j);
              return 1;
            }
            assert(target.cleared[k]<=1&&target.covered[k]<=1);
            retained+=!target.cleared[k]&&target.covered[k];
          }
          if(scale>=2&&dx==0&&dy==0)assert(retained>unsigned((clipped?cw*ch:w*h)*9/10));
          Target fast;fast.setClipRect(cx,cy,cw,ch);unsigned writes=0;
          const auto writer=[&](int left,int top,int width,int height,const uint16_t* data) {
            ++writes;assert(height<=16);
            for(int j=0;j<height;++j)for(int i=0;i<width;++i)
              fast.pixels[(top+j)*1280+left+i]=data[j*width+i];
          };
          stripe.front()=stripe.back()=0xbeef;
          const bool painted=orcsdr::maps::paint_scaled_rows(image.pixels.data(),w,h,{scale,dx,dy},x,y,
              cx,cy,cw,ch,stripe.data()+1,1280*16,writer);
          if(float(scale)!=2.f&&float(scale)!=4.f)assert(!painted&&writes==0);
          else {
            assert(painted&&writes>0);
            if(target.pixels!=fast.pixels) {
              std::fprintf(stderr,"PIXEL_MISMATCH scale=%g dx=%g dy=%g clipped=%d\n",scale,dx,dy,clipped);
              return 2;
            }
          }
          assert(stripe.front()==0xbeef&&stripe.back()==0xbeef);
          writes=0;
          assert(!orcsdr::maps::paint_scaled_rows(image.pixels.data(),w,h,{scale,dx,dy},x,y,
              cx,cy,cw,ch,stripe.data()+1,1,writer)&&writes==0);
          ++cases;
        }
  }
  std::printf("NATIVE_RASTER_PASS %u full-size scaled/offset cases; original pinned M5GFX raster bodies\n",cases);
}
'''
# Keep upstream attribution with the transient extracted implementation.
license_header = source[:source.index('#include')]
args.output.write_text(license_header + '\n' + harness, encoding='utf-8')
print('M5GFX_LGFXBase_cpp_SHA256=' + hashlib.sha256(vendor.read_bytes()).hexdigest())
