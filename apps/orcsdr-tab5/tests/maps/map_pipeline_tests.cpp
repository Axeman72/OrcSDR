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
#include <cstdlib>
#include <algorithm>

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
struct MeasuredText final : orcmap::LabelTextTarget {
  std::vector<std::string> names;
  bool MeasureText(std::string_view name,orcmap::TextExtent* extent) override {
    *extent={static_cast<int>(name.size())*12,28};return !name.empty();
  }
  void DrawText(int,int,std::string_view name,orcmap::Color,orcmap::Color) override {
    names.emplace_back(name);
  }
};
void global_label_test(const char* archive) {
  using namespace orcsdr::maps;
  orcmap::host::FileByteSource source(archive);
  Pipeline pipeline(source);
  assert(pipeline.open("protomaps-v4"));
  orcmap::Viewport view;
  view.width_px=1280;view.height_px=720;view.zoom=8;
  view.center_lat_deg=44.05;view.center_lon_deg=-123.02;
  orcmap::host::FramebufferTarget frame(1280,720);
  orcmap::LabelLayout labels;
  MeasuredText text;
  for (int zoom : {3,5}) {
    view.zoom=zoom;
    assert(pipeline.render(view,frame,&labels,&text));
    assert(pipeline.stats().labels.offered==0 && text.names.empty());
  }
  view.zoom=6;
  assert(pipeline.render(view,frame,&labels,&text));
  assert(pipeline.stats().labels.offered>0 && !text.names.empty());
  text.names.clear();view.zoom=3;
  assert(pipeline.render(view,frame,&labels,&text));
  assert(pipeline.stats().labels.offered==0 && text.names.empty());
  view.zoom=8;
  assert(pipeline.render(view,frame,&labels,&text));
  assert(pipeline.stats().tiles>0 && pipeline.stats().failed==0);
  assert(pipeline.stats().labels.offered>0 && pipeline.stats().labels.drawn>0);
  assert(pipeline.stats().labels.drawn<=64 && text.names.size()==pipeline.stats().labels.drawn);
  assert(std::find(text.names.begin(),text.names.end(),"Eugene")!=text.names.end());
  const auto names=text.names;const auto pixels=frame.Pixels();
  const auto features=pipeline.stats().features;
  text.names.clear();
  assert(pipeline.render(view,frame));
  assert(text.names.empty()&&pipeline.stats().labels.offered==0&&pipeline.stats().labels.drawn==0);
  assert(pipeline.stats().features==features&&frame.Pixels()==pixels); // text target records names only; all map geometry must remain.

  text.names.clear();
  assert(pipeline.render(view,frame,&labels,&text));
  assert(text.names==names && frame.Pixels()==pixels);
  text.names.clear();view.center_lat_deg=51.51;view.center_lon_deg=-0.13;
  assert(pipeline.render(view,frame,&labels,&text));
  assert(std::find(text.names.begin(),text.names.end(),"London")!=text.names.end());
  assert(std::find(text.names.begin(),text.names.end(),"Eugene")==text.names.end());
  std::printf("GLOBAL_STREAM_LABEL_PASS offered=%u drawn=%u\n",
              pipeline.stats().labels.offered,pipeline.stats().labels.drawn);
}
void inventory_tests(const char*);
void world_tests(const char*);
void screen_tests();
int main(int argc, char** argv) {
  assert(argc == 3 || argc == 4);
  screen_tests();
  if(argc==4)world_tests(argv[3]);
  inventory_tests(argv[2]);
  using namespace orcsdr::maps;
  orcmap::LocationPicker picker;
  assert(begin_location_picker(picker,900,330,440500000,-1230220000));
  assert(picker.View().width_px==900 && picker.View().height_px==330);
  assert(picker.Zoom()==picker.MinZoom()+1);
  assert(std::abs(picker.Selection().lat_deg-44.05)<1e-7);
  assert(std::abs(picker.Selection().lon_deg+123.022)<1e-7);
  const auto selected=picker.Selection();
  assert(picker.ZoomIn());
  assert(picker.ZoomOut());
  assert(picker.Selection().lat_deg==selected.lat_deg && picker.Selection().lon_deg==selected.lon_deg);
  assert(picker.RecenterAtScreen(550,165));
  assert(picker.Selection().lon_deg!=selected.lon_deg);
  assert(!begin_location_picker(picker,0,330,0,0));
  assert(!begin_location_picker(picker,900,330,900000001,0));
  orcmap::Viewport overview,requested;
  requested.zoom=3;requested.width_px=1248;requested.height_px=568;
  assert(world_overview_view(requested,0,4,&overview)&&overview.zoom==2&&overview.tile_size_px==512);
  assert(!world_overview_view(requested,4,4,&overview));
  assert(!world_overview_view(requested,0,4,nullptr));
  requested.zoom=20;requested.tile_size_px=INT32_MAX;
  assert(!world_overview_view(requested,0,4,&overview));
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
  orcmap::LabelLayout labels;
  assert(!pipeline.render(view,frame,&labels,nullptr)); // paired text capability required
  assert(pipeline.open("protomaps-v4")); // explicit, never infer from archive bytes
  assert(pipeline.open("openmaptiles-3.16"));
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
  if(const char* archive=std::getenv("ORCSDR_LABEL_TEST_ARCHIVE"))global_label_test(archive);
}
