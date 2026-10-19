#pragma once
#include "map_service.hpp"
#include "offline_map.hpp"

namespace orcsdr::maps {
// A returned view always carries the projection of the pixels actually drawn,
// including the explicit ORCMAP1 compatibility fallback.
struct RegionalView {
  orcmap::Viewport camera{};
  offline_map::View legacy{};
  FrameInfo frame{};
  bool valid = false, legacy_pixels = false;
};
RegionalView draw_region(lgfx::v1::LovyanGFX&, int32_t latitude_e7, int32_t longitude_e7,
                         double range_nm, int x, int y, int width, int height,
                         uint16_t background, uint16_t grid, uint16_t ink,
                         const char* preferred = nullptr, bool labels = true);
bool project(const RegionalView&, int32_t latitude_e7, int32_t longitude_e7, int* x, int* y);
} // namespace orcsdr::maps
