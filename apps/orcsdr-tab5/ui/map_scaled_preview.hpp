#pragma once
// Fixed-point sample/bounds math derives from M5GFX's LovyanGFX LGFXBase.cpp.
// Copyright (c) 2020 lovyan03; BSD-2-Clause terms and upstream notices:
// docs/licenses/LovyanGFX-license.txt and docs/licenses/M5GFX-LICENSE.txt.
#include "orcmap/frame_transform.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace orcsdr::maps {
constexpr size_t kPreviewStripeRows=16;
constexpr size_t kPreviewStripePixels=1280*kPreviewStripeRows;

// Native RGB565, opaque source. Batch rows through the public image API so an
// odd panel rotation does not turn every logical scanline into pixel-sized copies.
// The fixed-point sample positions match pinned M5GFX's nearest-neighbour raster.
template<class Writer>
bool paint_scaled_rows(const uint16_t* source,int width,int height,
    const orcmap::FrameTransform& transform,int x,int y,
    int clip_x,int clip_y,int clip_w,int clip_h,
    uint16_t* stripe,size_t capacity,Writer write) {
  if(!source||!stripe||width<=0||height<=0||width>1280||height>720||clip_w<=0||clip_h<=0)
    return false;
  // M5GFX accepts float scales; retain that exact rounding for fractional cameras.
  const float scale=static_cast<float>(transform.scale);
  // Shrinking already copies fewer pixels; keep its original sparse transfer.
  if(scale!=2.f&&scale!=4.f)
    return false;
  const int64_t left=std::max<int64_t>(x,clip_x),top=std::max<int64_t>(y,clip_y);
  const int64_t right=std::min<int64_t>(int64_t{x}+width,int64_t{clip_x}+clip_w);
  const int64_t bottom=std::min<int64_t>(int64_t{y}+height,int64_t{clip_y}+clip_h);
  if(right<=left||bottom<=top)return false;
  const auto output_width=static_cast<size_t>(right-left);
  const size_t rows=std::min(kPreviewStripeRows,capacity/output_width);
  if(!rows)return false;

  const float origin_x=(static_cast<float>(x+transform.dx)+0.5f)-0.5f*scale;
  const float origin_y=(static_cast<float>(y+transform.dy)+0.5f)-0.5f*scale;
  const int64_t step=static_cast<int64_t>(65536/scale);
  const auto fixed=[](float value,int64_t* result) {
    // Check in double so INT32_MAX does not round up to 2^31 as a float.
    if(!std::isfinite(value)||double(value)<std::numeric_limits<int32_t>::min()||
       double(value)>std::numeric_limits<int32_t>::max())return false;
    *result=static_cast<int64_t>(value);return true;
  };
  int64_t inverse_x=0,inverse_y=0,y_offset=0;
  if(!fixed(std::round(-origin_x*step),&inverse_x)||
     !fixed(std::round(-origin_y*step),&inverse_y)||
     !fixed(origin_y*65536.f+32768.f,&y_offset))return false;
  const auto floor16=[](int64_t value) {
    return value>=0?value/65536:-((-value+65535)/65536);
  };
  const int64_t source_x0=inverse_x+step/2,source_y0=inverse_y+step/2;
  const int64_t first_x=std::max(left,(source_x0+1-step)/-step);
  const int64_t end_x=std::min(right,(source_x0+1-int64_t{width}*65536-step)/-step);
  const int64_t first_y=std::max(top,floor16(y_offset-1));
  const int64_t end_y=std::min(bottom,floor16(y_offset+
      static_cast<int64_t>(scale*(height*65536))+1));
  const int64_t first_source_x=floor16(source_x0+first_x*step);
  const bool span_visible=first_x<end_x&&first_source_x>=0&&first_source_x<width;
  if(span_visible&&floor16(source_x0+(end_x-1)*step)>=width)return false;

  for(int64_t row=top;row<bottom;row+=static_cast<int64_t>(rows)) {
    const size_t count=std::min<size_t>(rows,bottom-row);
    std::fill_n(stripe,count*output_width,uint16_t{0x0841});
    for(size_t i=0;i<count;++i) {
      const int64_t destination_y=row+static_cast<int64_t>(i);
      if(!span_visible||destination_y<first_y||destination_y>=end_y)continue;
      const int64_t source_y=floor16(source_y0+destination_y*step);
      if(source_y<0||source_y>=height)continue;
      auto* output=stripe+i*output_width+static_cast<size_t>(first_x-left);
      int64_t source_x=source_x0+first_x*step;
      for(int64_t column=first_x;column<end_x;++column,source_x+=step)
        *output++=source[source_y*width+floor16(source_x)];
    }
    write(static_cast<int>(left),static_cast<int>(row),static_cast<int>(output_width),
          static_cast<int>(count),stripe);
  }
  return true;
}
}  // namespace orcsdr::maps
