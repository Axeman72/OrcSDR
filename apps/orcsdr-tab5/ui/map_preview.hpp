#pragma once
#include <string_view>
#include <algorithm>
#include "orcmap/frame_transform.hpp"
namespace orcsdr::maps {
// Call while holding the snapshot lock. Lifetime changes on cancel/rescan,
// not on camera movement, so an old camera can follow a new one in one session.
inline bool prepare_preview(const orcmap::Viewport& source,const orcmap::Viewport& destination,
    std::string_view source_pack,std::string_view destination_pack,uint32_t source_lifetime,
    uint32_t current_lifetime,bool valid,orcmap::FrameTransform* output) {
  if(!output||!valid||source_lifetime!=current_lifetime||source_pack!=destination_pack||
      source.width_px!=destination.width_px||source.height_px!=destination.height_px)return false;
  orcmap::FrameTransform result;
  if(!orcmap::MakeFrameTransform(source,destination,&result)||result.scale<0.25||result.scale>4)return false;
  *output=result;return true;
}
// Graphics adapter seam, shared by the native service and the recording test.
template<class Image,class Target>
void paint_preview(Image& image,Target& target,const orcmap::FrameTransform& transform,
                   int x,int y,int width,int height) {
  int32_t cx,cy,cw,ch;target.getClipRect(&cx,&cy,&cw,&ch);
  const int64_t left=std::max<int64_t>(x,cx),top=std::max<int64_t>(y,cy);
  const int64_t right=std::min<int64_t>(int64_t{x}+width,int64_t{cx}+cw);
  const int64_t bottom=std::min<int64_t>(int64_t{y}+height,int64_t{cy}+ch);
  if(width<=0||height<=0||cw<=0||ch<=0||right<=left||bottom<=top)return;
  target.setClipRect(left,top,right-left,bottom-top);
  target.fillRect(x,y,width,height,0x0841);
  if(transform.scale==1)image.pushSprite(&target,x+std::lround(transform.dx),y+std::lround(transform.dy));
  else {
    image.setPivot(0,0);
    image.pushRotateZoom(&target,x+transform.dx,y+transform.dy,0,transform.scale,transform.scale);
  }
  target.setClipRect(cx,cy,cw,ch);
}
}
