#include "map_pipeline.hpp"
#include "file_byte_source.hpp"
#include "framebuffer_target.hpp"
#include <cassert>
#include <cstdio>

void world_tests(const char* path) {
  orcmap::host::FileByteSource source(path);
  orcsdr::maps::Pipeline pipeline(source);
  assert(pipeline.open("orcmaps-overview-1"));
  assert(pipeline.header().min_zoom==0&&pipeline.header().max_zoom==4);
  orcmap::Viewport view;view.width_px=900;view.height_px=330;view.zoom=2;
  orcmap::host::FramebufferTarget frame(900,330);
  for(double longitude:{-123.022,0.0,179.99}) {
    view.center_lat_deg=44.05;view.center_lon_deg=longitude;
    assert(pipeline.render(view,frame));
    const auto pixels=frame.Pixels();
    assert(pipeline.render(view,frame));assert(frame.Pixels()==pixels);
    std::printf("WORLD_VIEW lon=%.3f tiles=%u features=%u bytes=%llu\n",longitude,
        pipeline.stats().tiles,pipeline.stats().features,
        static_cast<unsigned long long>(pipeline.stats().bytes));
  }
  assert(frame.WritePpm("map-world-antimeridian.ppm"));
  std::puts("MAP_WORLD_TEST_PASS (host geometry; not a P4 performance result)");
}
