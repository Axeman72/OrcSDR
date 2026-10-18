// Real picker/Settings UI and camera; only display, device storage and navigation hooks are faked.
#include "map_screen.hpp"
#include "map_service.hpp"
#include "location_store.hpp"
#include "receiver_location.hpp"
#include "settings_app.hpp"
#include "navigation_service.hpp"
#include "dashboard_audio_control.hpp"
#include "text_editor.hpp"
#include <M5Unified.h>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <map>
#include <string>
#include <vector>
namespace {
std::map<std::string,std::vector<unsigned char>> nvs_data;
bool write_ok=true;
orcsdr::NvsStore preferences;
orcmap::Viewport requested;
int saves=0;
std::string chosen_pack;
bool select_pack(const char* identity) {chosen_pack=identity;return true;}
bool visible(const char* value) {
  return std::find(M5.Display.strings.begin(),M5.Display.strings.end(),value)!=M5.Display.strings.end();
}
orcsdr::settings::State settings_state() {
  orcsdr::settings::State state;
  const auto location=orcsdr::receiver_location::snapshot();
  state.location_configured=location.configured;
  state.latitude_e7=location.latitude_e7;state.longitude_e7=location.longitude_e7;
  state.radar_range_nm=25;
  return state;
}
bool save(int32_t latitude,int32_t longitude) {
  ++saves;
  if(!orcsdr::location_store::save(preferences,{true,latitude,longitude}))return false;
  orcsdr::receiver_location::set(true,latitude,longitude);return true;
}
void click(int x,int y) {
  orcsdr::map_screen::touch(x,y,true,true);
  orcsdr::map_screen::touch(x,y,false,false);
  orcsdr::map_screen::tick();
}
}
namespace orcsdr {
NvsStore::~NvsStore()=default;
bool NvsStore::is_key(const char* key) const {return nvs_data.count(key)!=0;}
size_t NvsStore::bytes_length(const char* key) const {return is_key(key)?nvs_data.at(key).size():0;}
size_t NvsStore::get_bytes(const char* key,void* out,size_t size) const {
  if(bytes_length(key)!=size)return 0;std::memcpy(out,nvs_data.at(key).data(),size);return size;
}
bool NvsStore::put_bytes(const char* key,const void* value,size_t size) {
  if(!write_ok)return false;const auto* bytes=static_cast<const unsigned char*>(value);
  nvs_data[key]={bytes,bytes+size};return true;
}
uint32_t NvsStore::get_u32(const char* key,uint32_t fallback) const {uint32_t value;return get_bytes(key,&value,4)?value:fallback;}
int32_t NvsStore::get_i32(const char* key,int32_t fallback) const {int32_t value;return get_bytes(key,&value,4)?value:fallback;}
bool NvsStore::get_bool(const char* key,bool fallback) const {bool value;return get_bytes(key,&value,1)?value:fallback;}
bool NvsStore::put_u32(const char* key,uint32_t value) {return put_bytes(key,&value,4);}
namespace navigation {
void open_settings(settings::Section section) {
  screens::begin_transition(screens::Id::settings,millis(),true);
  settings::enter(settings_state(),section);screens::finish_transition();
}
void close_settings() {settings::leave();screens::close_settings(millis());screens::finish_transition();}
}
namespace maps {
void cancel() {}
uint32_t revision() {return 0;}
bool snapshot(MapSnapshot* out) {
  *out=MapSnapshot{};out->loaded=true;out->sd=true;out->count=6;out->revision=1;
  for(int i=0;i<6;++i) {auto& pack=out->packs[i];std::snprintf(pack.title,sizeof(pack.title),"Map %d",i);std::snprintf(pack.identity,sizeof(pack.identity),"pack-%d",i);pack.usable=true;}
  return true;
}
bool running() {return true;}
void rescan() {}
Frame draw(const orcmap::Viewport& view,lgfx::LovyanGFX&,int,int,FrameInfo*,const char*) {
  requested=view;return Frame::pending;
}
}
namespace audio_header {
void draw_brand(const char*) {} void draw_battery(int32_t) {}
void draw_mute_button(bool) {} void draw_visualizer_button(bool) {} void draw_settings_button() {}
}
namespace text_editor {
void begin(const char*,const char*,size_t,bool,const char*) {} void draw() {} void close() {}
const char* value() {return "";}
bool active() {return false;}
Result handle_touch(int,int) {return Result::none;}
keyboard_input::EditAction handle_key(const keyboard_input::Key&) {return keyboard_input::EditAction::none;}
void accept_point(int* x,int* y) {*x=*y=0;} void cancel_point(int* x,int* y) {*x=*y=0;}
}
}
void screen_tests() {
  using namespace orcsdr;
  receiver_location::set(true,440000000,-1230000000);
  navigation::open_settings(settings::Section::location_adsb);
  assert(visible("CHOOSE LOCATION ON MAP")&&!visible("ADS-B RADAR RANGE"));
  assert(settings::handle_touch(500,380).kind==settings::ActionKind::open_location_picker);
  auto external=settings_state();external.latitude_e7=450000000;
  M5.Display.strings.clear();settings::update(external);
  assert(visible("45.0000000")); // Previously changed state silently without repainting.
  map_screen::configure({nullptr,save,nullptr});
  map_screen::open(map_screen::Mode::picker);
  assert(requested.zoom==3&&requested.width_px==1248&&requested.height_px==568);
  const auto original=requested;
  click(205,170); // right half-screen
  assert(requested.center_lon_deg!=original.center_lon_deg);
  const auto moved=requested;
  click(1220,110);assert(requested.zoom==4);
  assert(requested.center_lat_deg==moved.center_lat_deg&&requested.center_lon_deg==moved.center_lon_deg);
  click(1220,175);assert(requested.zoom==3);
  click(130,110);assert(requested.center_lat_deg>moved.center_lat_deg);
  click(130,240);assert(std::abs(requested.center_lat_deg-moved.center_lat_deg)<1e-7);
  click(55,170);assert(std::abs(requested.center_lon_deg-original.center_lon_deg)<1e-7);
  click(205,170);
  const auto selected=requested;
  write_ok=false;click(1100,675);
  assert(saves==1&&screens::owns(screens::Id::maps));
  assert(receiver_location::snapshot().longitude_e7==-1230000000);
  assert(visible("Location could not be saved. Please retry."));
  write_ok=true;click(1100,675);
  assert(saves==2&&screens::owns(screens::Id::settings));
  const auto saved=location_store::load(preferences);
  assert(saved.configured&&saved.longitude==std::llround(selected.center_lon_deg*1e7));
  assert(settings::state().longitude_e7==saved.longitude&&saved.longitude!=-1230000000);
  char decimal[32];std::snprintf(decimal,sizeof(decimal),"%.7f",saved.longitude/1e7);
  assert(visible(decimal));
  map_screen::open(map_screen::Mode::picker);click(205,170);click(1130,40);
  assert(saves==2&&settings::state().longitude_e7==saved.longitude);
  receiver_location::set(false,0,0);
  map_screen::open(map_screen::Mode::picker);click(1260,707);
  assert(saves==3&&screens::owns(screens::Id::settings)); // Center can be saved without first moving the map.
  receiver_location::set(true,900000000,0);
  map_screen::open(map_screen::Mode::picker);click(1260,707);
  assert(saves==4&&receiver_location::snapshot().latitude_e7==900000000);
  receiver_location::set(true,440000000,-1230000000);
  map_screen::open(map_screen::Mode::setup);
  click(1000,670);click(1000,670);
  assert(visible("Set your receiver location"));
  assert(visible("NETWORK LOCATION")&&visible("SEARCH ZIP / ADDRESS"));
  click(200,420);assert(visible("USE LOCATION"));
  click(1130,40);
  map_screen::configure({nullptr,save,select_pack});
  map_screen::open(map_screen::Mode::inventory);
  assert(visible("Map 0")&&visible("Map 3")&&!visible("Map 4"));
  map_screen::touch(300,450,true,true);map_screen::touch(300,300,true,false);map_screen::touch(300,300,false,false);map_screen::tick();
  assert(visible("Map 4")&&!visible("Map 0"));
  click(300,450);click(900,540);assert(chosen_pack=="pack-4");
  navigation::open_settings(settings::Section::location_adsb);
  assert(settings::begin_location_method(0).kind==settings::ActionKind::location_ip_lookup);
  std::puts("MAP_SCREEN_TEST_PASS (changed-point save/return, failure, cancel, pan, zoom, Settings redraw)");
}
