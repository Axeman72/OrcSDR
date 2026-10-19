#include "map_preview.hpp"
#include <cassert>
struct Target {
  int32_t x=5,y=5,w=20,h=20;
  int fills=0;
  void getClipRect(int32_t* a,int32_t* b,int32_t* c,int32_t* d){*a=x;*b=y;*c=w;*d=h;}
  void setClipRect(int32_t a,int32_t b,int32_t c,int32_t d){x=a;y=b;w=c;h=d;}
  void fillRect(int,int,int,int,int){++fills;assert(x==10&&y==10&&w==15&&h==15);}
};
struct Image {
  int pushes=0,scales=0;
  void pushSprite(Target* target,int,int){++pushes;assert(target->x==10&&target->w==15);}
  void setPivot(int,int){}
  void pushRotateZoom(Target* target,double,double,int,double,double){++scales;assert(target->y==10&&target->h==15);}
};
int main() {
  using namespace orcsdr::maps;
  orcmap::Viewport a;a.zoom=8;a.width_px=1248;a.height_px=568;
  auto b=a;assert(orcmap::PanByPixels(&b,25,10));
  orcmap::FrameTransform result;
  assert(prepare_preview(a,b,"regional","regional",7,7,true,&result));
  assert(!prepare_preview(a,b,"regional","regional",7,8,true,&result)); // Late old publication after cancel.
  assert(!prepare_preview(a,b,"regional","other",7,7,true,&result));
  assert(!prepare_preview(a,b,"regional","regional",7,7,false,&result)); // Allocation failed / reclaimed.
  b.width_px=900;assert(!prepare_preview(a,b,"","",7,7,true,&result));
  b=a;b.zoom=11;assert(!prepare_preview(a,b,"","",7,7,true,&result));
  b.zoom=10;assert(prepare_preview(a,b,"","",7,7,true,&result)&&result.scale==4);
  assert(!prepare_preview(a,b,"","",7,7,true,nullptr));
  Target target;Image image;
  paint_preview(image,target,{1,-30,40},10,10,30,30);
  assert(image.pushes==1&&target.fills==1&&target.x==5&&target.y==5&&target.w==20&&target.h==20);
  paint_preview(image,target,{2,0,0},10,10,30,30);
  assert(image.scales==1&&target.x==5&&target.w==20);
  paint_preview(image,target,{1,0,0},100,100,30,30);
  assert(target.fills==2&&image.pushes==1); // Empty clip neither clears nor paints.
}
