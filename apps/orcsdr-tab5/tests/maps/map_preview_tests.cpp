#include "map_preview.hpp"
#include <cassert>
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
}
