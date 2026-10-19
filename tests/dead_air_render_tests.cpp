// Compiles the actual Dead Air M5GFX renderer with the existing ACARS
// recording stand-in; no hardware or simulated live RF data.
#include <M5Unified.h>
#include "dead_air_dashboard.hpp"
#include "screen_controller.hpp"
#include <cassert>
#include <string>
#include <cstdio>
using namespace orcsdr;
namespace da = orcsdr::dead_air;
namespace screen = orcsdr::screens;

static bool displayed(const char* text) {
  for (const Prim& p : host_display::prims)
    if (p.kind == Prim::text && p.str.find(text) != std::string::npos) return true;
  return false;
}
static void validate_bounds() {
  for (const Prim& p : host_display::prims) {
    int x, y, w, h;
    if (!host_display::effective(p, &x, &y, &w, &h)) continue;
    assert(x >= 0 && y >= 0 && x + w <= 1280 && y + h <= 720);
  }
}
int main() {
  da::EventModel model;
  model.reset();
  da::attach_events(&model);
  da::Snapshot snap{};
  snap.revision = 1;
  snap.battery_percent = -1;
  // Never admit cached values from an absent ENV III or detached receiver.
  snap.temperature_c = {99.0f, 0, 0, true, true, true};
  snap.environment_sensor_attached = false;
  snap.rf_level_dbfs = {-14.0f, 0, 0, true, true, true};
  snap.receiver_ready = false;
  screen::begin_transition(screen::Id::dead_air, 100);
  da::enter(snap);
  screen::finish_transition();
  assert(da::active() && da::page() == da::Page::overview);
  assert(displayed("MODULE NOT CONNECTED"));
  assert(!displayed("99.0") && !displayed("-14.0"));
  assert(displayed("RECEIVER UNAVAILABLE"));
  validate_bounds();
  for (int i = 0; i < 6; ++i) {
    host_display::reset();
    const int x = 20 + i * 210 + 40;
    auto action = da::handle_touch(x, 673, 1000 + i);
    assert(action.kind == da::ActionKind::none);
    assert(da::page() == static_cast<da::Page>(i));
    // An already active tab must not cause a redundant screen repaint.
    if (i == 0) assert(host_display::prims.empty());
    else assert(!host_display::prims.empty());
    validate_bounds();
  }
  // Sensitivity buttons/marker are real touch actions only on Events.
  da::handle_touch(20 + 4*210 + 40, 673, 2000);
  assert(da::page() == da::Page::events);
  da::handle_touch(1140, 400, 2100);
  assert(model.sensitivity() == da::Sensitivity::sensitive);
  da::handle_touch(900, 530, 2200);
  assert(model.count() == 1 && model.newest(0)->source == da::Source::manual);
  assert(displayed("INVESTIGATOR MARK"));
  // Mode and step buttons act only while a receiver is actually present, and only on the Spectrum tab.
  da::handle_touch(20 + 1 * 210 + 40, 673, 2300);
  assert(da::page() == da::Page::spectrum);
  assert(da::handle_touch(145, 370, 2310).kind == da::ActionKind::none);
  snap.receiver_ready = true;
  snap.receiver_settled = true;
  snap.frequency_hz = 98700000;
  std::snprintf(snap.demod, sizeof(snap.demod), "WFM");
  snap.revision = 5;
  da::update(snap);
  auto mode = da::handle_touch(145, 370, 2320);
  assert(mode.kind == da::ActionKind::set_demod && mode.value == 4 && std::string(da::kModeNames[4]) == "LSB");
  mode = da::handle_touch(40 + 95 + 10, 290 + 20, 2330);
  assert(mode.kind == da::ActionKind::set_demod && mode.value == 1);
  assert(da::handle_touch(200, 470, 2340).kind == da::ActionKind::tune_up);
  assert(da::handle_touch(60, 470, 2350).kind == da::ActionKind::tune_down);
  validate_bounds();
  // Once another screen takes the framebuffer, Dead Air must be silent.
  screen::begin_transition(screen::Id::home, 3000);
  screen::finish_transition();
  host_display::reset();
  snap.revision = 2;
  da::update(snap);
  assert(host_display::prims.empty());
  assert(da::handle_touch(900, 530, 3200).kind == da::ActionKind::none);
  da::leave();
  assert(!da::active());
  std::puts("Dead Air renderer host tests PASS");
}
