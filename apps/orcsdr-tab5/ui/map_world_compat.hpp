#pragma once
#include "map_pipeline.hpp"
namespace lgfx { inline namespace v1 { class LovyanGFX; } }
namespace orcsdr::maps {
// Compatibility for whole-globe Maidenhead displays, including polar grids.
// OrcMaps currently supplies Mercator only. Keep this explicit until its core
// supports a whole-globe projection; do not misalign locators over a new base.
void draw_world_compat(lgfx::v1::LovyanGFX&,int sub_x,int sub_y,int left,int top,int width,int height);
}
