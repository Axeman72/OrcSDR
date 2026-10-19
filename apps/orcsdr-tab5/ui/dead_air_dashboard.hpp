#pragma once
#include "dead_air_model.hpp"
#include <cstdint>

namespace orcsdr::dead_air {

enum class ActionKind : uint8_t {
  none, close, toggle_mute, open_settings, start_scan, stop_scan,
  start_recording, stop_recording, save_clip, export_session, play_clip,
  tune_up, tune_down, set_demod, sweep_preset, sweep_dwell
};
// Real receiver demodulators only. Sweep patterns and generated audio are not demodulators.
constexpr const char* kModeNames[] = {"NFM", "WFM", "AM", "USB", "LSB"};
constexpr int kModeCount = 5;

struct Action { ActionKind kind = ActionKind::none; int32_t value = 0; };

// All of these entry points run on the OrcSDR UI task. They do not own
// receiver, USB, DSP, I2C or SD hardware. Live observations are supplied
// through Snapshot by an application-level adapter.
void attach_events(EventModel* model);
void enter(const Snapshot& snapshot);
void update(const Snapshot& snapshot);
void leave();
bool active();
Page page();
Action handle_touch(int32_t x, int32_t y, uint32_t now_ms);
bool self_check();

} // namespace orcsdr::dead_air
