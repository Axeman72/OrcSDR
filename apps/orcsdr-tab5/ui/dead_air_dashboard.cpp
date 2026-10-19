#include "dead_air_dashboard.hpp"
#include "dashboard_audio_control.hpp"
#include "screen_controller.hpp"
#include <M5Unified.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace orcsdr::dead_air {
namespace {
constexpr uint16_t kBg = TFT_BLACK, kPanel = 0x0841, kEdge = 0x2945,
  kGreen = 0x07e0, kCyan = 0x2e7f, kViolet = 0xa15f,
  kMuted = 0x8c71, kYellow = 0xffe0, kRed = 0xf800;
constexpr int kTabY = 649, kTabX = 20, kTabWidth = 198, kTabGap = 12, kTabHeight = 52;
constexpr int kBodyY = 114, kBodyBottom = 635;
EventModel* g_events = nullptr;
Snapshot g_snapshot{};
Page g_page = Page::overview;
bool g_active = false;

bool hit(int x, int y, int rx, int ry, int rw, int rh) {
  return x >= rx && y >= ry && x < rx + rw && y < ry + rh;
}
void label(const char* value, int x, int y, uint16_t color = TFT_WHITE, uint8_t scale = 2) {
  M5.Display.setTextDatum(middle_left);
  M5.Display.setTextSize(scale);
  M5.Display.setTextColor(color);
  M5.Display.drawString(value, x, y);
}
void box(int x, int y, int w, int h, uint16_t border = kEdge) {
  M5.Display.fillRoundRect(x, y, w, h, 8, kPanel);
  M5.Display.drawRoundRect(x, y, w, h, 8, border);
}
void heading(const char* text, int x, int y) { label(text, x, y, kCyan, 2); }
void divider(int x, int y, int width) { M5.Display.drawFastHLine(x, y, width, kEdge); }
void button(const char* text, int x, int y, int w, uint16_t accent, bool enabled) {
  box(x, y, w, 52, enabled ? accent : kEdge);
  label(text, x + 16, y + 26, enabled ? TFT_WHITE : kMuted);
}
void reading(const char* name, const Reading& value, int x, int y, const char* format) {
  char output[32] = "--";
  const Status state = reading_status(value);
  if (state == Status::current) snprintf(output, sizeof(output), format, static_cast<double>(value.value));
  label(name, x, y, kMuted);
  label(output, x + 230, y, state == Status::current ? TFT_WHITE : kMuted, 2);
  if (state == Status::stale) label("STALE", x + 400, y, kYellow, 1);
}
void plot(const float* data, size_t count, bool valid, int x, int y, int w, int h,
          float minimum, float maximum, uint16_t color) {
  M5.Display.drawRect(x, y, w, h, kEdge);
  for (int t = 1; t < 4; ++t)
    M5.Display.drawFastHLine(x + 1, y + h * t / 4, w - 2, kEdge);
  if (!valid || count < 2 || !(maximum > minimum)) {
    label("NO LIVE PLOT DATA", x + 16, y + h / 2, kMuted, 2);
    return;
  }
  const auto py = [&](float v) {
    if (!std::isfinite(v)) v = minimum;
    const float n = std::clamp((v - minimum) / (maximum - minimum), 0.0f, 1.0f);
    return y + h - 2 - static_cast<int>(n * (h - 4));
  };
  for (size_t i = 1; i < count; ++i) {
    const int xa = x + 1 + static_cast<int>((i - 1) * (w - 3) / (count - 1));
    const int xb = x + 1 + static_cast<int>(i * (w - 3) / (count - 1));
    M5.Display.drawLine(xa, py(data[i - 1]), xb, py(data[i]), color);
  }
}
void nav() {
  static constexpr const char* tabs[] = {"Overview", "Spectrum", "Audio", "Environment", "Events", "Review"};
  M5.Display.fillRect(0, 641, 1280, 79, kBg);
  for (uint8_t i = 0; i < static_cast<uint8_t>(Page::count); ++i) {
    const int x = kTabX + i * (kTabWidth + kTabGap);
    box(x, kTabY, kTabWidth, kTabHeight, g_page == static_cast<Page>(i) ? kGreen : kEdge);
    label(tabs[i], x + 17, kTabY + 26,
          g_page == static_cast<Page>(i) ? kGreen : TFT_WHITE, 2);
  }
}
void chrome() {
  M5.Display.setFont(nullptr);
  M5.Display.fillScreen(kBg);
  audio_header::draw_brand("DEAD AIR");
  audio_header::draw_battery(g_snapshot.battery_percent);
  audio_header::draw_home_button();
  audio_header::draw_mute_button(true);
  audio_header::draw_visualizer_button(false);
  audio_header::draw_settings_button();
  nav();
}
void overview() {
  box(20, 120, 612, 252);
  heading("INVESTIGATION", 40, 143);
  label(g_snapshot.recording ? "RECORDING" : "READY", 40, 190,
        g_snapshot.recording ? kRed : kGreen, 3);
  char buffer[52];
  snprintf(buffer, sizeof(buffer), "ELAPSED %02lu:%02lu:%02lu",
    static_cast<unsigned long>(g_snapshot.session_elapsed_s / 3600),
    static_cast<unsigned long>(g_snapshot.session_elapsed_s / 60 % 60),
    static_cast<unsigned long>(g_snapshot.session_elapsed_s % 60));
  label(buffer, 40, 240, TFT_WHITE);
  if (g_snapshot.receiver_ready) {
    snprintf(buffer, sizeof(buffer), "%.3f MHz  %s", g_snapshot.frequency_hz / 1e6,
             g_snapshot.demod);
    label(buffer, 40, 291, kCyan, 3);
  } else label("RECEIVER UNAVAILABLE", 40, 291, kYellow, 2);
  label(g_snapshot.recording ? "STOP IN SESSION CONTROLS" : "RECORDING REQUIRES ADAPTER",
        40, 343, kMuted, 1);
  box(646, 120, 614, 252);
  heading("LIVE OBSERVATIONS", 668, 143);
  reading("RF (dBFS)", g_snapshot.rf_level_dbfs, 670, 192, "%.1f");
  reading("AUDIO (dBFS)", g_snapshot.audio_level_dbfs, 670, 247, "%.1f");
  divider(666, 283, 565);
  label("Sensitivity", 670, 321, kMuted);
  label(g_events && g_events->sensitivity() == Sensitivity::quiet ? "QUIET" :
    g_events && g_events->sensitivity() == Sensitivity::sensitive ? "SENSITIVE" : "STANDARD",
    935, 321, kGreen);
  box(20, 390, 612, 241);
  heading("LATEST EVENTS", 40, 413);
  if (!g_events || !g_events->count()) label("No recorded observations yet", 40, 483, kMuted);
  else for (size_t i = 0; i < std::min<size_t>(4, g_events->count()); ++i) {
    const Event* e = g_events->newest(i); if (!e) continue;
    char row[86];
    snprintf(row, sizeof(row), "#%lu  %s: %s", static_cast<unsigned long>(e->sequence),
             source_name(e->source), kind_name(e->kind));
    label(row, 40, 462 + i * 38, i == 0 ? kGreen : TFT_WHITE, 1);
  }
  box(646, 390, 614, 241);
  heading("ENV III / OPTIONAL", 668, 413);
  label(g_snapshot.environment_sensor_attached ? "MODULE DETECTED" : "MODULE NOT CONNECTED",
        668, 464, g_snapshot.environment_sensor_attached ? kGreen : kMuted, 2);
  reading("TEMP (C)", g_snapshot.temperature_c, 668, 505, "%.1f");
  reading("RH (%%)", g_snapshot.humidity_rh, 668, 550, "%.1f");
  reading("PRESSURE (hPa)", g_snapshot.pressure_hpa, 668, 594, "%.1f");
}
void spectrum() {
  box(20, 120, 312, 511);
  heading("TUNING", 40, 144);
  char hz[48];
  if (g_snapshot.receiver_ready) snprintf(hz, sizeof(hz), "%.3f MHz", g_snapshot.frequency_hz / 1e6);
  else snprintf(hz, sizeof(hz), "NO RECEIVER");
  label(hz, 40, 201, kGreen, 3);
  divider(40, 248, 271);
  label("DEMODULATION", 40, 276, kMuted, 2);
  label(g_snapshot.receiver_ready ? g_snapshot.demod : "--", 40, 325, TFT_WHITE, 3);
  label("STEP / SCAN CONTROL", 40, 395, kMuted, 2);
  label("Awaiting receiver adapter", 40, 445, kYellow, 2);
  label("No guessed frequencies", 40, 542, kMuted, 1);
  box(346, 120, 914, 511);
  heading("RF SPECTRUM  /  dBFS", 366, 144);
  if (g_snapshot.receiver_ready) label(hz, 986, 144, kGreen, 2);
  plot(g_snapshot.spectrum_dbfs, kBins,
       g_snapshot.receiver_ready && g_snapshot.spectrum_valid,
       373, 177, 862, 292, -120.0f, 0.0f, kGreen);
  heading("WATERFALL", 366, 509);
  label("Awaiting real IQ history (no synthetic waterfall)", 377, 570, kMuted);
}
void audio() {
  box(20, 120, 1240, 251);
  heading("DEMODULATED AUDIO  /  NORMALIZED ENVELOPE", 40, 143);
  plot(g_snapshot.audio_envelope, kBins, g_snapshot.audio_available &&
       g_snapshot.audio_envelope_valid, 42, 171, 1194, 180, -1.0f, 1.0f, kCyan);
  box(20, 388, 773, 243);
  heading("AUDIO ANALYSIS", 40, 415);
  reading("LEVEL (dBFS)", g_snapshot.audio_level_dbfs, 40, 485, "%.1f");
  label("No automatic speech claims", 40, 560, kMuted);
  box(809, 388, 451, 243);
  heading("SOURCE", 829, 415);
  label(g_snapshot.receiver_ready ? g_snapshot.demod : "NO AUDIO SOURCE", 829, 482, TFT_WHITE, 2);
  label("Clip replay requires recorder adapter", 829, 551, kMuted, 1);
}
void environment() {
  box(20, 120, 610, 511);
  heading("ENV III / OPTIONAL SENSOR", 40, 144);
  label(g_snapshot.environment_sensor_attached ? "CONNECTED" : "UNAVAILABLE",
        40, 208, g_snapshot.environment_sensor_attached ? kGreen : kMuted, 3);
  reading("TEMPERATURE (C)", g_snapshot.temperature_c, 40, 280, "%.1f");
  reading("HUMIDITY (%%)", g_snapshot.humidity_rh, 40, 355, "%.1f");
  reading("PRESSURE (hPa)", g_snapshot.pressure_hpa, 40, 430, "%.1f");
  label("Core RF investigation works without ENV III.", 40, 548, kMuted, 1);
  box(644, 120, 616, 511);
  heading("SENSOR INTEGRITY", 666, 144);
  label("No derived score without validated baselines", 666, 201, kMuted, 1);
  reading("DEVICE MOTION (g)", g_snapshot.motion_g, 666, 271, "%.3f");
  label("This describes Tab5 motion, NOT room movement.", 666, 331, kMuted, 1);
  label("Temperature / humidity / pressure", 666, 426, kCyan, 2);
  label("shown only from current valid sensor samples", 666, 476, kMuted, 1);
}
void events() {
  box(20, 120, 767, 511);
  heading("EVENT LOG / LAST 7", 40, 144);
  if (!g_events || !g_events->count()) {
    label("NO EVENTS RECORDED", 43, 266, kMuted, 3);
  } else {
    const size_t rows = std::min<size_t>(7, g_events->count());
    for (size_t i = 0; i < rows; ++i) {
      const Event* e = g_events->newest(i); if (!e) continue;
      char line[94];
      snprintf(line, sizeof(line), "#%lu %s / %s",
        static_cast<unsigned long>(e->sequence), source_name(e->source), kind_name(e->kind));
      label(line, 40, 206 + static_cast<int>(i) * 55,
        e->source == Source::manual ? kViolet : TFT_WHITE, 2);
      if (i != rows - 1) divider(40, 233 + static_cast<int>(i) * 55, 718);
    }
  }
  box(801, 120, 459, 511);
  heading("DETECTION", 822, 144);
  label("AUTO RF LEVEL RISE", 822, 211, kCyan);
  const Sensitivity sens = g_events ? g_events->sensitivity() : Sensitivity::standard;
  char threshold[64];
  snprintf(threshold, sizeof(threshold), "Threshold +%.0f dB", rf_rise_threshold_db(sens));
  label(threshold, 822, 272, TFT_WHITE, 3);
  label(g_events && g_events->baseline_ready() ? "BASELINE READY" : "BASELINE WARMING",
        822, 333, g_events && g_events->baseline_ready() ? kGreen : kYellow, 2);
  button("QUIET", 818, 382, 135, kCyan, true);
  button("NORMAL", 956, 382, 135, kCyan, true);
  button("HIGH", 1094, 382, 145, kCyan, true);
  button("MARK EVENT", 818, 505, 420, kRed, g_events != nullptr);
}
void review() {
  box(20, 120, 460, 511);
  heading("SESSION ARCHIVE", 40, 144);
  label("Recorder not connected", 40, 240, kYellow);
  label("No clips will be fabricated", 40, 300, kMuted);
  box(497, 120, 763, 511);
  heading("EVIDENCE REVIEW", 517, 144);
  plot(nullptr, 0, false, 519, 195, 718, 248, -1.0f, 1.0f, kGreen);
  label("WAV / IQ clip indexing and hashes", 519, 498, kMuted);
  label("will appear after the storage adapter is wired.", 519, 550, kMuted);
}
void draw_body() {
  M5.Display.fillRect(0, kBodyY, 1280, kBodyBottom - kBodyY + 1, kBg);
  switch (g_page) {
    case Page::overview: overview(); break;
    case Page::spectrum: spectrum(); break;
    case Page::audio: audio(); break;
    case Page::environment: environment(); break;
    case Page::events: events(); break;
    case Page::review: review(); break;
    default: break;
  }
}
} // namespace

void attach_events(EventModel* events) { g_events = events; }
void enter(const Snapshot& snapshot) {
  g_snapshot = snapshot;
  g_page = Page::overview;
  g_active = true;
  chrome();
  draw_body();
}
void update(const Snapshot& snapshot) {
  if (!g_active || snapshot.revision == g_snapshot.revision) return;
  const int battery = g_snapshot.battery_percent;
  g_snapshot = snapshot;
  if (battery != snapshot.battery_percent) audio_header::draw_battery(snapshot.battery_percent);
  draw_body();
}
void leave() { g_active = false; }
bool active() { return g_active; }
Page page() { return g_page; }
Action handle_touch(int32_t x, int32_t y, uint32_t now_ms) {
  if (!g_active) return {};
  if (audio_header::home_hit(x, y)) return {ActionKind::close};
  if (audio_header::mute_hit(x, y)) return {ActionKind::toggle_mute};
  if (audio_header::settings_hit(x, y)) return {ActionKind::open_settings};
  if (y >= kTabY && y < kTabY + kTabHeight) {
    for (uint8_t i = 0; i < static_cast<uint8_t>(Page::count); ++i) {
      if (hit(x, y, kTabX + i * (kTabWidth + kTabGap), kTabY, kTabWidth, kTabHeight)) {
        if (g_page != static_cast<Page>(i)) {
          g_page = static_cast<Page>(i);
          nav(); draw_body();
        }
        return {};
      }
    }
  }
  if (g_page == Page::events && g_events != nullptr) {
    if (hit(x, y, 818, 505, 420, 52)) {
      g_events->mark_manual(now_ms, g_snapshot.unix_time, g_snapshot.frequency_hz);
      draw_body(); return {};
    }
    if (hit(x, y, 818, 382, 421, 52)) {
      g_events->set_sensitivity(x < 956 ? Sensitivity::quiet :
                                 x < 1094 ? Sensitivity::standard : Sensitivity::sensitive);
      draw_body(); return {};
    }
  }
  return {};
}
bool self_check() {
  return static_cast<uint8_t>(Page::count) == 6 && kTabX > 0 &&
    kTabX + 5 * (kTabWidth + kTabGap) + kTabWidth <= 1280 &&
    kBodyBottom < kTabY && kBodyY >= 110;
}

} // namespace orcsdr::dead_air
