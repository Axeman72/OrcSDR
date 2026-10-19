#pragma once
#include <string_view>
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
}
