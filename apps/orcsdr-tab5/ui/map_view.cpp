#include "map_view.hpp"
#include <M5GFX.h>
#include <cstdio>
#include <cstring>
#include <algorithm>

namespace orcsdr::maps {
RegionalView draw_region(lgfx::v1::LovyanGFX& display,int32_t latitude,int32_t longitude,
                         double range,int x,int y,int width,int height,
                         uint16_t background,uint16_t grid,uint16_t ink,const char* preferred) {
  RegionalView result;
  result.valid=regional_view(latitude,longitude,range,width,height,&result.camera);
  if(!result.valid)return result;
  result.legacy={latitude/1e7f,longitude/1e7f,static_cast<float>(range),x,y,width,height};
  display.fillRect(x,y,width,height,background);
  result.frame.state=draw(result.camera,display,x,y,&result.frame,preferred);
  if(result.frame.state!=Frame::ready) {
    // Legacy data remains usable without conversion. Keep its projection with
    // its basemap, rather than putting Mercator markers over equirectangular data.
    result.legacy_pixels=offline_map::available();
    if(result.legacy_pixels) {
      offline_map::draw_base(display,result.legacy,0x0320,grid,ink,grid);
      std::snprintf(result.frame.title,sizeof(result.frame.title),"Legacy regional map (ORCMAP1)");
      std::snprintf(result.frame.credit,sizeof(result.frame.credit),"OpenStreetMap contributors, ODbL");
    } else display.drawRect(x,y,width,height,grid);
    std::snprintf(result.frame.status,sizeof(result.frame.status),"%s",
        result.frame.state==Frame::pending ? "LOADING LOCAL MAP" : "NO BASEMAP - COORDINATES ONLY");
  }
  display.setTextSize(1);display.setTextDatum(bottom_left);display.setTextColor(ink,background);
  const char* caption=result.frame.credit[0] ? result.frame.credit : result.frame.status;
  // Full required attribution, wrapped within the map instead of truncated.
  const int columns=width/6;
  if(columns>0) {
    const size_t length=std::strlen(caption);
    const int lines=static_cast<int>((length+columns-1)/columns);
    for(int line=0;line<lines;++line) {
      char part[193]{};
      const size_t count=std::min(static_cast<size_t>(columns),length-line*columns);
      std::memcpy(part,caption+line*columns,count);
      display.drawString(part,x+2,y+height-2-(lines-1-line)*10);
    }
  }
  return result;
}
bool project(const RegionalView& view,int32_t latitude,int32_t longitude,int* x,int* y) {
  if(!view.valid || !x || !y || latitude<-900000000 || latitude>900000000 ||
      longitude<-1800000000 || longitude>1800000000)return false;
  if(view.legacy_pixels)return offline_map::project(view.legacy,latitude/1e7f,longitude/1e7f,x,y);
  if(!project(view.camera,latitude,longitude,x,y))return false;
  *x+=view.legacy.x;*y+=view.legacy.y;return true;
}
} // namespace orcsdr::maps
