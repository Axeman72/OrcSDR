#include "map_screen.hpp"
#include "map_service.hpp"
#include "receiver_location.hpp"
#include "setup_flow.hpp"
#include "screen_controller.hpp"
#include "focus_nav.hpp"
#include "navigation_service.hpp"
#include "settings_app.hpp"
#include "orcmap/location_picker.hpp"
#include <M5Unified.h>
#include <esp_attr.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace orcsdr::map_screen {
namespace {
constexpr uint16_t kBg=0x080e,kInk=0xc618,kAccent=0x07ff;
constexpr int kMapX=32,kMapY=220,kMapW=900,kMapH=330;
Hooks g_hooks;
NvsStore* g_preferences=nullptr;
setup::Flow g_saved,g_flow;
Mode g_mode=Mode::inventory;
bool g_auto=false,g_dirty=true,g_drag=false,g_has_proposal=false;
bool g_lat_set=false,g_lon_set=false,g_return_settings=false;
screens::Id g_return=screens::Id::home;
settings::Section g_section=settings::Section::data_maps;
int g_last_x=0,g_last_y=0,g_edit=0,g_index=0;
int32_t g_lat=0,g_lon=0;
char g_entry[16]{},g_message[96]{};
orcmap::LocationPicker g_picker;
EXT_RAM_BSS_ATTR maps::MapSnapshot g_packs;
uint32_t g_last_map_revision=0,g_last_tick=0;

bool hit(int x,int y,int bx,int by,int w,int h) {return x>=bx&&x<bx+w&&y>=by&&y<by+h;}
void text(const char* s,int x,int y,int size=2,uint16_t color=kInk) {
  M5.Display.setTextDatum(top_left);M5.Display.setTextSize(size);M5.Display.setTextColor(color,kBg);
  M5.Display.drawString(s,x,y);
}
void button(const char* label,int x,int y,int w=180,int h=46) {
  focus_nav::note(x,y,w,h);M5.Display.drawRoundRect(x,y,w,h,6,kAccent);text(label,x+12,y+14,2,kAccent);
}
void wrapped(const char* value,int x,int y,int columns=100) {
  const size_t length=std::strlen(value);
  for(size_t offset=0;offset<length;offset+=columns) {
    char part[193]{};std::memcpy(part,value+offset,std::min(static_cast<size_t>(columns),length-offset));
    text(part,x,y,1);y+=16;
  }
}
bool checkpoint() {
  // A rerun stays in RAM until Finish; Cancel retains the old completion record.
  if(!g_auto)return true;
  if(g_flow.writable&&g_preferences&&g_preferences->putUInt("maps_setup",g_flow.record()))return true;
  std::snprintf(g_message,sizeof(g_message),"Could not save setup progress. Existing settings retained.");return false;
}
bool location_step() {return g_mode==Mode::picker||(g_mode==Mode::setup&&g_flow.step==setup::Step::location);}
bool maps_step() {return g_mode==Mode::inventory||(g_mode==Mode::setup&&g_flow.step==setup::Step::maps);}
void advance() {g_flow.next();(void)checkpoint();g_dirty=true;}
void picker_at(int32_t latitude,int32_t longitude) {
  orcmap::Viewport view;view.width_px=kMapW;view.height_px=kMapH;
  g_picker.Begin(view,{-85.05112878,-180,85.05112878,180},{0,20});
  double x=0,y=0;
  if(orcmap::ProjectLatLon(g_picker.View(),{latitude/1e7,longitude/1e7},&x,&y))g_picker.RecenterAtScreen(x,y);
}
void moved_picker() {
  const auto point=g_picker.Selection();g_lat=static_cast<int32_t>(std::llround(point.lat_deg*1e7));
  g_lon=static_cast<int32_t>(std::llround(point.lon_deg*1e7));
  g_lat_set=g_lon_set=g_has_proposal=true;g_dirty=true;
}
void begin_edit(int field) {
  g_edit=field;std::snprintf(g_entry,sizeof(g_entry),"%ld",static_cast<long>(field==1?g_lat:g_lon));g_dirty=true;
}
void finish_edit() {
  int32_t value=0;
  if(!setup::parse_e7(g_entry,g_edit==1,&value)) {
    std::snprintf(g_message,sizeof(g_message),"Invalid E7 coordinate (degrees x 10000000).");g_dirty=true;return;
  }
  if(g_edit==1) {g_lat=static_cast<int32_t>(value);g_lat_set=true;}
  else {g_lon=static_cast<int32_t>(value);g_lon_set=true;}
  g_edit=0;g_has_proposal=g_lat_set&&g_lon_set;picker_at(g_lat,g_lon);g_dirty=true;g_message[0]=0;
}
void draw_map() {
  g_last_map_revision=maps::revision();
  if(std::abs(static_cast<int64_t>(g_lat))>850511287) {
    M5.Display.fillRect(kMapX,kMapY,kMapW,kMapH,kBg);
    text("Polar latitude: no Mercator basemap. The entered E7 coordinate is retained.",44,228,1);
    return;
  }
  const auto location=receiver_location::snapshot();maps::FrameInfo info;
  const auto state=maps::draw(g_picker.View(),M5.Display,kMapX,kMapY,&info,location.map_pack);
  if(state!=maps::Frame::ready) {
    M5.Display.fillRect(kMapX,kMapY,kMapW,kMapH,kBg);
    M5.Display.drawRect(kMapX,kMapY,kMapW,kMapH,0x2945);
    text(state==maps::Frame::pending?"Loading offline map...":"No basemap installed. Pan or enter coordinates to choose a location.",44,228,1);
  } else wrapped(info.credit,44,528,140);
  M5.Display.drawLine(kMapX+kMapW/2-14,kMapY+kMapH/2,kMapX+kMapW/2+14,kMapY+kMapH/2,kAccent);
  M5.Display.drawLine(kMapX+kMapW/2,kMapY+kMapH/2-14,kMapX+kMapW/2,kMapY+kMapH/2+14,kAccent);
}
void draw_picker() {
  text("Choose a receiver location. The crosshair is a proposal; USE LOCATION saves it.",32,110);
  char value[64];std::snprintf(value,sizeof(value),"LAT E7  %ld",static_cast<long>(g_lat));button(value,32,150,440);
  std::snprintf(value,sizeof(value),"LON E7  %ld",static_cast<long>(g_lon));button(value,500,150,432);
  button("ZOOM +",970,240,230);button("ZOOM -",970,305,230);
  button("USE LOCATION",970,470,260);
  text(g_has_proposal?"Explicit confirmation required.":"No location selected yet.",32,570);
  text("Drag / tap to pan. Keyboard: Ctrl+arrows pans; +/- zooms. E7 = degrees x 10000000.",32,602,1);
  draw_map();
}
void draw_inventory() {
  (void)maps::snapshot(&g_packs);
  text(g_packs.sd?"SD inventory: /sd/orcmaps (separate from signed downloads)":"No SD card. Firmware overview remains independent.",32,110);
  if(g_packs.overflow)text("Too many map manifests (limit 16). No regional packs selected.",32,144,2,TFT_ORANGE);
  if(!maps::running())text("Map service unavailable. Manual E7 location remains available.",32,180);
  else if(!g_packs.loaded)text("Scanning local maps...",32,180);
  if(g_packs.out_of_memory)text("Scan ran out of memory. Rescan to retry; existing settings are retained.",32,570,1,TFT_ORANGE);
  if(g_packs.count) {
    g_index=std::clamp(g_index,0,static_cast<int>(g_packs.count)-1);
    const auto& pack=g_packs.packs[g_index];char line[128];
    std::snprintf(line,sizeof(line),"%d / %u   %s",g_index+1,g_packs.count,pack.title);text(line,32,180);
    wrapped(pack.identity,32,226,128);text(pack.region,32,280);
    std::snprintf(line,sizeof(line),"Version %s   zoom %u-%u   %llu bytes",pack.version[0]?pack.version:"unknown",pack.min_zoom,pack.max_zoom,static_cast<unsigned long long>(pack.bytes));
    text(line,32,324);text(pack.status,32,370,2,pack.usable?kAccent:TFT_ORANGE);
    wrapped(pack.credit,32,416,128);
    text(pack.coverage,32,462,1);wrapped(pack.provenance,32,482,128);
    if(pack.usable)button("USE THIS MAP",800,520,250);
  }
  button("PREV",32,520);button("NEXT",230,520);button("RESCAN",440,520);
  text("Discovery checks metadata and size. Archive SHA-256 is not yet verified on this device.",32,590,1);
}
void draw_edit() {
  M5.Display.fillRect(0,90,1280,540,kBg);
  text(g_edit==1?"LATITUDE E7 (-900000000 to 900000000)":"LONGITUDE E7 (-1800000000 to 1800000000)",40,110);
  text(g_entry,40,160,3,kAccent);
  constexpr char keys[]="123456789-0<";
  for(int i=0;i<12;++i) {char label[2]{keys[i],0};button(label,400+(i%3)*120,220+(i/3)*80,100,60);}
  button("SAVE FIELD",820,300,250);button("CANCEL FIELD",820,400,250);
}
void close_screen() {
  g_edit=0;maps::cancel();screens::begin_transition(g_return,millis());screens::finish_transition();
  if(g_return_settings)navigation::open_settings(g_section);
  else if(g_hooks.restore)g_hooks.restore(g_return);
}
void finish() {
  const auto previous=g_flow;g_flow.done=true;
  if(!g_flow.writable||!g_preferences||!g_preferences->putUInt("maps_setup",g_flow.record())) {
    g_flow=previous;std::snprintf(g_message,sizeof(g_message),"Setup record could not be saved (unknown schema or storage error).");g_dirty=true;return;
  }
  g_saved=g_flow;g_saved.automatic=false;g_auto=false;
  close_screen();
}
void cancel() {close_screen();}
} // namespace
void configure(const Hooks& hooks) {g_hooks=hooks;}
void load(NvsStore& preferences) {
  g_preferences=&preferences;
  const bool legacy=preferences.isKey("journal")||preferences.isKey("workflow")||
      preferences.isKey("loc_point")||
      preferences.isKey("adsb_loc_set")||preferences.isKey("adsb_lat_e7")||
      preferences.isKey("map_pack")||preferences.isKey("wifi0_ssid")||
      preferences.isKey("wifi_ssid")||preferences.isKey("set_bright")||preferences.isKey("pair_key");
  g_saved=setup::Flow::restore(preferences.isKey("maps_setup"),preferences.getUInt("maps_setup",0),legacy);
  if(g_saved.automatic) {
    // Mark the blank install before legacy boot defaults are written.
    g_auto=true;g_flow=g_saved;(void)checkpoint();g_auto=false;
  }
}
bool first_run_pending() {return g_saved.automatic;}
void open(Mode mode,bool automatic) {
  const bool nested=screens::is_active(screens::Id::maps)||
      (settings::active()&&screens::status().return_to==screens::Id::maps);
  if(!nested) {g_return_settings=settings::active();g_section=settings::section();}
  // Close the original Settings session first, restoring its graphics state.
  // Optional Network opens a new session that can safely return to this screen.
  if(settings::active())navigation::close_settings();
  if(!nested)g_return=screens::status().active;
  screens::begin_transition(screens::Id::maps,millis());
  g_mode=mode;g_auto=automatic;g_flow=automatic?g_saved:setup::Flow{};
  g_flow.writable=g_saved.writable;g_saved.automatic=false;
  g_edit=0;g_message[0]=0;g_dirty=true;g_index=0;g_drag=false;
  const auto location=receiver_location::snapshot();g_lat=location.latitude_e7;g_lon=location.longitude_e7;
  g_lat_set=g_lon_set=g_has_proposal=location.configured;
  picker_at(g_lat,g_lon);maps::cancel();draw();screens::finish_transition();
}
void draw() {
  if(!screens::is_active(screens::Id::maps))return;
  M5.Display.setFont(nullptr);M5.Display.fillScreen(kBg);focus_nav::clear();
  text(g_mode==Mode::setup?"ORCSDR SETUP":g_mode==Mode::picker?"RECEIVER LOCATION":"LOCAL MAPS",32,28,3,kAccent);
  button("CANCEL",1050,20,195);
  if(location_step())draw_picker();
  else if(maps_step())draw_inventory();
  else if(g_flow.step==setup::Step::welcome) {
    text("Welcome to OrcSDR",32,150,3);text("Set up an optional network, your receiver location, and offline maps.",32,220);
    text("Existing location, radio settings, and SD files are retained.",32,270);button("QUICK START",32,390,260);
  } else if(g_flow.step==setup::Step::network) {
    text("Network is optional. Offline maps and manual location work without Wi-Fi.",32,150);
    button("NETWORK SETTINGS",32,260,350);
  } else text("Setup complete. You can rerun it from Settings > System.",32,180);
  if(g_mode==Mode::setup) {
    if(g_flow.step!=setup::Step::welcome)button("BACK",32,650);
    button(g_flow.step==setup::Step::complete?"FINISH":"NEXT / SKIP",920,650,310);
  }
  if(g_edit)draw_edit();
  if(g_message[0])text(g_message,32,625,1,TFT_ORANGE);
  g_dirty=false;
}
void tick() {
  if(!screens::owns(screens::Id::maps))return;
  if(g_dirty)draw();
  const uint32_t now=millis();
  if(now-g_last_tick<250)return;
  g_last_tick=now;
  if(location_step()&&!g_edit&&maps::revision()!=g_last_map_revision) {
    draw_map();
  }
  if(maps_step()) {
    const auto previous=g_packs.revision;
    if(maps::snapshot(&g_packs)&&previous!=g_packs.revision)g_dirty=true;
  }
}
void touch(int x,int y,bool pressed,bool new_press) {
  if(!screens::owns(screens::Id::maps))return;
  if(!pressed) {g_drag=false;return;}
  if(g_edit) {
    if(!new_press)return;
    if(hit(x,y,820,300,250,46))finish_edit();
    else if(hit(x,y,820,400,250,46)) {g_edit=0;g_dirty=true;}
    else {
      constexpr char keys[]="123456789-0<";
      for(int i=0;i<12;++i)if(hit(x,y,400+(i%3)*120,220+(i/3)*80,100,60)) {
        keyboard_input::Key key;if(keys[i]=='<')key.special=keyboard_input::Special::backspace;else key.ch=keys[i];
        (void)keyboard_input::apply_edit(g_entry,sizeof(g_entry),11,key);g_dirty=true;
      }
    }
    return;
  }
  if(location_step()&&hit(x,y,kMapX,kMapY,kMapW,kMapH)) {
    if(new_press) {g_picker.RecenterAtScreen(x-kMapX,y-kMapY);g_drag=true;g_last_x=x;g_last_y=y;moved_picker();}
    else if(g_drag&&(x!=g_last_x||y!=g_last_y)) {g_picker.DragByGesture(x-g_last_x,y-g_last_y);g_last_x=x;g_last_y=y;moved_picker();}
    return;
  }
  if(!new_press)return;
  if(hit(x,y,1050,20,195,46)) {cancel();return;}
  if(g_mode==Mode::setup) {
    if(hit(x,y,32,650,180,46)) {g_flow.back();(void)checkpoint();g_dirty=true;return;}
    if(hit(x,y,920,650,310,46)) {if(g_flow.step==setup::Step::complete)finish();else advance();return;}
    if(g_flow.step==setup::Step::welcome&&hit(x,y,32,390,260,46)) {g_flow.quick_start();(void)checkpoint();g_dirty=true;return;}
    if(g_flow.step==setup::Step::network&&hit(x,y,32,260,350,46)) {navigation::open_settings(settings::Section::connectivity);return;}
  }
  if(location_step()) {
    if(hit(x,y,32,150,440,46))begin_edit(1);
    else if(hit(x,y,500,150,432,46))begin_edit(2);
    else if(hit(x,y,970,240,230,46)) {g_picker.ZoomIn();g_dirty=true;}
    else if(hit(x,y,970,305,230,46)) {g_picker.ZoomOut();g_dirty=true;}
    else if(hit(x,y,970,470,260,46)&&g_has_proposal&&g_hooks.location) {
      if(g_hooks.location(g_lat,g_lon)) {if(g_mode==Mode::setup)advance();else cancel();}
      else {std::snprintf(g_message,sizeof(g_message),"Location could not be saved. Please retry.");g_dirty=true;}
    }
  } else if(maps_step()) {
    if(hit(x,y,32,520,180,46)) {g_index=std::max(0,g_index-1);g_dirty=true;}
    else if(hit(x,y,230,520,180,46)) {g_index=std::min(std::max(0,static_cast<int>(g_packs.count)-1),g_index+1);g_dirty=true;}
    else if(hit(x,y,440,520,180,46)) {maps::rescan();g_packs.loaded=false;g_dirty=true;}
    else if(hit(x,y,800,520,250,46)&&g_packs.count&&g_hooks.pack) {
      g_index=std::clamp(g_index,0,static_cast<int>(g_packs.count)-1);
      const auto& pack=g_packs.packs[g_index];
      if(pack.usable&&g_hooks.pack(pack.identity)) {if(g_mode==Mode::setup)advance();else cancel();}
    }
  }
}
void key(const keyboard_input::Key& key) {
  using keyboard_input::Special;
  if(g_edit) {
    const auto action=keyboard_input::apply_edit(g_entry,sizeof(g_entry),11,key);
    if(action==keyboard_input::EditAction::accept)finish_edit();
    else if(action==keyboard_input::EditAction::cancel) {g_edit=0;g_dirty=true;}
    else if(action==keyboard_input::EditAction::edited)g_dirty=true;
    return;
  }
  if(key.special==Special::escape) {cancel();return;}
  if(location_step()&&(key.ch=='+'||key.ch=='-')) {
    if(key.ch=='+')g_picker.ZoomIn();else g_picker.ZoomOut();g_dirty=true;return;
  }
  if(location_step()&&key.ctrl) {
    int dx=0,dy=0;
    if(key.special==Special::left)dx=40;else if(key.special==Special::right)dx=-40;
    else if(key.special==Special::up)dy=40;else if(key.special==Special::down)dy=-40;
    if(dx||dy) {g_picker.DragByGesture(dx,dy);moved_picker();return;}
  }
  focus_nav::Direction direction;
  bool move=true;
  if(key.special==Special::left)direction=focus_nav::Direction::left;
  else if(key.special==Special::right)direction=focus_nav::Direction::right;
  else if(key.special==Special::up)direction=focus_nav::Direction::up;
  else if(key.special==Special::down||key.special==Special::tab)direction=focus_nav::Direction::down;
  else move=false;
  if(move&&focus_nav::move(direction)) {
    focus_nav::Rect rect;if(focus_nav::focused(&rect))M5.Display.drawRect(rect.x-2,rect.y-2,rect.w+4,rect.h+4,TFT_YELLOW);
  } else if(key.special==Special::enter) {
    focus_nav::Rect rect;if(focus_nav::focused(&rect))touch(rect.x+rect.w/2,rect.y+rect.h/2,true,true);
  }
}
} // namespace orcsdr::map_screen
