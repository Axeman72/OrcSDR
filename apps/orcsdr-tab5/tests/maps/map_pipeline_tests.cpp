#include "map_pipeline.hpp"
#include "file_byte_source.hpp"
#include "framebuffer_target.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <utility>
#include <new>

struct FailingSource final : orcmap::ByteSource {
  explicit FailingSource(orcmap::ByteSource& source):source(source) {}
  orcmap::ByteSource& source;
  bool fail=false;
  size_t Read(uint64_t offset,void* bytes,size_t count) override {
    if(fail)throw std::bad_alloc();
    return source.Read(offset,bytes,count);
  }
  uint64_t Size() const override {return source.Size();}
  bool Valid() const override {return source.Valid();}
};

struct Empty final : orcmap::ByteSource {
  size_t Read(uint64_t, void*, size_t) override { return 0; }
  uint64_t Size() const override { return 0; }
  bool Valid() const override { return false; }
};
bool cancel(void*) { return false; }
void inventory_tests(const char*);
void world_tests(const char*);
int main(int argc, char** argv) {
  assert(argc == 3 || argc == 4);
  if(argc==4)world_tests(argv[3]);
  inventory_tests(argv[2]);
  using namespace orcsdr::maps;
  for (auto dimensions : {std::pair<int,int>{842,330}, {480,320}, {320,240}}) {
    orcmap::Viewport view;
    assert(regional_view(440500000,-1230220000,15,dimensions.first,dimensions.second,&view));
    int x=-1,y=-1;
    assert(project(view,440500000,-1230220000,&x,&y));
    assert(std::abs(x-dimensions.first/2)<=1 && std::abs(y-dimensions.second/2)<=1);
    assert(!project(view,INT32_MAX,0,&x,&y));
  }
  orcmap::Viewport view;
  assert(regional_view(0,1799900000,50,480,320,&view));
  int x=0,y=0;
  assert(world_compat_project(0,0,50,158,824,420,&x,&y)&&x==462&&y==368);
  assert(world_compat_project(90,-180,50,158,824,420,&x,&y)&&x==50&&y==158);
  assert(world_compat_project(-90,180,50,158,824,420,&x,&y)&&x==874&&y==578);
  assert(!world_compat_project(91,0,0,0,480,320,&x,&y));
  assert(!world_compat_project(0,NAN,0,0,480,320,&x,&y));
  assert(project(view,0,-1799900000,&x,&y)); // wrap across the antimeridian
  assert(!regional_view(900000001,0,15,480,320,&view));
  assert(!regional_view(900000000,0,15,480,320,&view));
  assert(!project(view,900000000,0,&x,&y));
  assert(!regional_view(0,0,0,480,320,&view));
  assert(!regional_view(0,0,std::numeric_limits<double>::infinity(),480,320,&view));
  assert(!regional_view(0,0,15,0,320,&view));
  assert(!regional_view(0,0,15,480,320,nullptr));

  Empty empty;
  Pipeline bad(empty);
  assert(!bad.open("openmaptiles-3.16"));
  orcmap::host::FileByteSource source(argv[1]);
  Pipeline pipeline(source);
  assert(!pipeline.open("unsupported-profile"));
  assert(pipeline.open("openmaptiles-3.16"));
  view={}; view.width_px=view.height_px=view.tile_size_px=64;
  orcmap::host::FramebufferTarget frame(64,64);
  assert(pipeline.render(view,frame));
  assert(pipeline.stats().tiles==1 && pipeline.stats().failed==0);
  assert(pipeline.stats().features>0 && pipeline.stats().bytes>0);
  assert(frame.WritePpm("map-pipeline-golden.ppm"));
  const auto pixels=frame.Pixels();
  assert(pipeline.render(view,frame));
  assert(pixels==frame.Pixels()); // repeated viewport produces identical pixels
  FailingSource failing(source);Pipeline resilient(failing);
  failing.fail=true;
  assert(!resilient.open("openmaptiles-3.16")&&resilient.stats().out_of_memory);
  failing.fail=false;assert(resilient.open("openmaptiles-3.16"));
  failing.fail=true;
  assert(!resilient.render(view,frame)&&resilient.stats().out_of_memory);
  failing.fail=false;assert(resilient.open("openmaptiles-3.16"));
  assert(resilient.render(view,frame));assert(frame.Pixels()==pixels);
  view.zoom=31;
  assert(!pipeline.render(view,frame));
  view.zoom=0;view.width_px=65;
  assert(!pipeline.render(view,frame));

  Pipeline cancelled(source,{nullptr,cancel,nullptr});
  assert(cancelled.open("openmaptiles-3.16"));
  view.width_px=64;
  assert(!cancelled.render(view,frame) && cancelled.stats().cancelled);
  std::puts("MAP_PIPELINE_TEST_PASS");
}
