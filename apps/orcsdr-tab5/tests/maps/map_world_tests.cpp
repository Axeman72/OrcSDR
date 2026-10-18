#include "map_pipeline.hpp"
#include "file_byte_source.hpp"
#include "framebuffer_target.hpp"
#include <cassert>
#include <cstdio>
#include <cmath>
#include <utility>

void world_tests(const char* path) {
  orcmap::host::FileByteSource source(path);
  orcsdr::maps::Pipeline pipeline(source);
  assert(pipeline.open("orcmaps-overview-1"));
  assert(pipeline.header().min_zoom==0&&pipeline.header().max_zoom==4);
  for(auto size:{std::pair<int,int>{900,330},{1248,568}}) {
    orcmap::Viewport view;
    orcmap::host::FramebufferTarget frame(size.first,size.second);
    for(double longitude:{-123.022,0.0,179.99}) {
      orcmap::LocationPicker picker;
      assert(orcsdr::maps::begin_location_picker(picker,size.first,size.second,440500000,
          static_cast<int32_t>(std::llround(longitude*1e7))));
      view=picker.View();
      int x=0,y=0;
      assert(orcsdr::maps::project(view,440500000,static_cast<int32_t>(std::llround(longitude*1e7)),&x,&y));
      assert(std::abs(x-size.first/2)<=1 && std::abs(y-size.second/2)<=1);
      assert(pipeline.render(view,frame));
      const auto pixels=frame.Pixels();
      assert(pipeline.render(view,frame));assert(frame.Pixels()==pixels);
      std::printf("WORLD_VIEW size=%dx%d lon=%.3f tiles=%u features=%u bytes=%llu\n",size.first,size.second,longitude,
          pipeline.stats().tiles,pipeline.stats().features,
          static_cast<unsigned long long>(pipeline.stats().bytes));
    }
    assert(frame.WritePpm(size.first==1248?"map-world-large.ppm":"map-world-antimeridian.ppm"));
  }
  std::puts("MAP_WORLD_TEST_PASS (host geometry; not a P4 performance result)");
}
