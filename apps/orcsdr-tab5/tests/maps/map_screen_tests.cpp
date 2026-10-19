// Real picker/Settings UI and camera; only display, device storage and navigation hooks are faked.
#include "map_screen.hpp"
#include "map_service.hpp"
#include "location_store.hpp"
#include "receiver_location.hpp"
#include "settings_app.hpp"
#include "navigation_service.hpp"
#include "dashboard_audio_control.hpp"
#include "text_editor.hpp"
#include "setup_ui.hpp"
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
bool pause_ok=true,setup_paused=false;
int pause_calls=0,resume_calls=0;
bool setup_reception(bool pause) {
  if(pause) {++pause_calls;if(!pause_ok)return false;setup_paused=true;}
  else {++resume_calls;setup_paused=false;}
  return true;
}
orcsdr::NvsStore preferences;
orcmap::Viewport requested;
int saves=0;
std::string chosen_pack;
orcsdr::maps::Frame map_result=orcsdr::maps::Frame::pending;
bool select_pack(const char* identity) {chosen_pack=identity;return true;}
bool visible(const char* value) {
  return std::find(M5.Display.strings.begin(),M5.Display.strings.end(),value)!=M5.Display.strings.end();
}
orcsdr::settings::State settings_state() {
  orcsdr::settings::State state;
  const auto location=orcsdr::receiver_location::snapshot();
  state.location_configured=location.configured;
  state.latitude_e7=location.latitude_e7;state.longitude_e7=location.longitude_e7;
  strlcpy(state.location_label,location.label,sizeof(state.location_label));
  state.radar_range_nm=25;
  return state;
}
bool save(int32_t latitude,int32_t longitude) {
  assert(!M5.Display.strings.empty()&&M5.Display.strings.back()=="SAVING...");
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
  requested=view;return map_result;
}
}
namespace audio_header {
void draw_brand(const char*) {} void draw_battery(int32_t) {}
void draw_mute_button(bool) {} void draw_visualizer_button(bool) {} void draw_settings_button() {}
}

}
void screen_tests() {
  using namespace orcsdr;
  receiver_location::set(true,440000000,-1230000000);
  navigation::open_settings(settings::Section::location_adsb);
  assert(visible("CHOOSE LOCATION ON MAP")&&!visible("ADS-B RADAR RANGE"));
  assert(visible("LOCATION")&&visible("SAVED LOCATION")&&!visible("PROFILE LABEL"));
  auto named=settings_state();strlcpy(named.location_label,"Springfield, Oregon",sizeof(named.location_label));
  M5.Display.strings.clear();settings::update(named);
  assert(visible("Springfield, Oregon")&&!visible("SAVED LOCATION"));
  auto unset=named;unset.location_configured=false;
  M5.Display.strings.clear();settings::update(unset);
  assert(visible("NOT SET")&&!visible("Springfield, Oregon"));
  M5.Display.strings.clear();settings::update(settings_state());
  assert(visible("SAVED LOCATION"));
  assert(settings::handle_touch(500,380).kind==settings::ActionKind::open_location_picker);
  auto external=settings_state();external.latitude_e7=450000000;
  const std::string long_pack(200,'x');
  std::snprintf(external.map_pack,sizeof(external.map_pack),"%s",long_pack.c_str());
  M5.Display.strings.clear();settings::update(external);
  assert(!visible(long_pack.c_str()));
  assert(visible((std::string(39,'x')+"...").c_str()));
  assert(std::string(settings::state().map_pack)==long_pack);
  assert(visible("45.0000000")); // Previously changed state silently without repainting.
  map_screen::configure({nullptr,save,nullptr});
  map_screen::open(map_screen::Mode::picker);
  assert(requested.zoom==3&&requested.width_px==1248&&requested.height_px==568);
  const auto original=requested;
  map_result=maps::Frame::ready;map_screen::draw();
  const int clears=M5.Display.screen_clears,map_clears=M5.Display.map_clears;
  map_result=maps::Frame::pending;
  click(205,170);
  assert(M5.Display.screen_clears==clears&&M5.Display.map_clears==map_clears);
  assert(visible("Updating map..."));
  map_screen::open(map_screen::Mode::picker);
  map_screen::touch(800,400,true,true);map_screen::tick();
  assert(requested.center_lon_deg==original.center_lon_deg);
  map_screen::touch(820,400,true,false);map_screen::tick();
  assert(requested.center_lon_deg<original.center_lon_deg);
  const auto dragged=requested;
  map_screen::touch(820,400,false,false);map_screen::tick();
  assert(requested.center_lon_deg==dragged.center_lon_deg);
  map_screen::open(map_screen::Mode::picker);
  click(800,400);assert(requested.center_lon_deg>original.center_lon_deg);
  map_screen::open(map_screen::Mode::picker);
  map_screen::touch(800,400,true,true);
  map_screen::touch(802,402,true,false);map_screen::tick();
  assert(requested.center_lon_deg==original.center_lon_deg); // Small jitter is not a drag.
  map_screen::touch(802,402,false,false);map_screen::tick();
  assert(requested.center_lon_deg>original.center_lon_deg);
  map_screen::open(map_screen::Mode::picker);
  map_screen::touch(800,400,true,true);
  map_screen::touch(800,700,false,false);map_screen::tick();
  assert(requested.center_lon_deg==original.center_lon_deg); // Release outside cancels a tap.
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
  assert(visible("USE LOCATION")&&!visible("SAVING...")); // Failed write restores a retry action.
  write_ok=true;click(1100,675);
  assert(saves==2&&screens::owns(screens::Id::settings));
  const auto saved=location_store::load(preferences);
  assert(saved.configured&&saved.longitude==std::llround(selected.center_lon_deg*1e7));
  assert(settings::state().longitude_e7==saved.longitude&&saved.longitude!=-1230000000);
  assert(visible("SAVED LOCATION"));
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
  assert(visible("Set receiver location"));
  assert(visible("USE NETWORK LOCATION")&&visible("SEARCH ZIP / ADDRESS"));
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
  map_screen::load(preferences);
  map_screen::configure({nullptr,save,select_pack,nullptr,setup_reception});
  map_screen::open(map_screen::Mode::setup);
  assert(setup_paused&&pause_calls==1);
  assert(visible("Reception paused during setup."));
  click(1000,670);click(1000,670);
  assert(setup_paused&&resume_calls==0); // Wi-Fi/location steps retain the pause.
  click(1130,40);
  assert(!setup_paused&&resume_calls==1);
  map_screen::open(map_screen::Mode::setup);
  click(100,410); // Quick Start goes to Ready.
  write_ok=false;click(1000,670);
  assert(setup_paused&&resume_calls==1); // Failed Finish must retain the pause.
  write_ok=true;
  click(1000,670);
  assert(!setup_paused&&resume_calls==2);
  map_screen::open(map_screen::Mode::setup);
  setup_ui::configure({+[](setup_ui::Snapshot* out){
    out->count=1;strlcpy(out->networks[0].ssid,"Bench",33);out->networks[0].secure=true;
  },nullptr,nullptr,nullptr,nullptr});
  setup_ui::touch(setup_ui::Page::network,100,260);
  assert(setup_ui::input_active()&&text_editor::active());
  keyboard_input::Key secret;secret.ch='x';setup_ui::key(setup_ui::Page::network,secret);
  screens::begin_transition(screens::Id::home,millis());screens::finish_transition();
  map_screen::tick();
  assert(!text_editor::active()&&std::string(text_editor::value()).empty());
  // A later tick cannot close a newer editor owned by another screen.
  text_editor::begin("OTHER OWNER","keep",20,false,"SAVE");map_screen::tick();
  assert(text_editor::active()&&std::string(text_editor::value())=="keep");text_editor::close();
  assert(!setup_paused&&resume_calls==3); // External Home must release too.
  pause_ok=false;
  map_screen::open(map_screen::Mode::setup);
  assert(!screens::owns(screens::Id::maps)&&!setup_paused);
  pause_ok=true;
  map_screen::open(map_screen::Mode::picker);
  assert(!setup_paused&&pause_calls==4); // Ordinary map use never pauses reception.
  std::puts("MAP_SCREEN_TEST_PASS (changed-point save/return, failure, cancel, pan, zoom, Settings redraw)");
}
