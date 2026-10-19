#include "dead_air_dashboard.hpp"
#include "dashboard_audio_control.hpp"
#include "screen_controller.hpp"
#include <M5Unified.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <new>
#include <utility>
#include <chrono>
#if defined(ESP_PLATFORM)
#include <esp_timer.h>
#endif
#include "acars_dirty_rects.hpp"
#include "dead_air_sweep.hpp"
#include "dead_air_palette.hpp"

namespace orcsdr::dead_air {
namespace {
constexpr uint16_t kBg = TFT_BLACK, kPanel = 0x0841, kEdge = 0x2945,
  kGreen = 0x07e0, kCyan = 0x2e7f, kViolet = 0xa15f,
  kMuted = 0x8c71, kYellow = 0xffe0, kRed = 0xf800;
constexpr int kTabY = 649, kTabX = 20, kTabWidth = 198, kTabGap = 12, kTabHeight = 52;
constexpr int kBodyY = 114, kBodyBottom = 635;
constexpr int kModeX0 = 40, kModeY0 = 290, kModeDx = 95, kModeDy = 60, kModeW = 86;
constexpr int kStepY = 452;
constexpr int kSweepY = 530;
EventModel* g_events = nullptr;
Snapshot* g_snapshot = nullptr;  // heap, not static: internal .data/.bss below the DMA-pool margin aborts boot
Page g_page = Page::overview;
bool g_active = false;

// Body panels are drawn into an off-screen canvas and pushed once per update, so a refresh never shows a
// cleared frame (that was the flicker). Drawing code keeps absolute screen coordinates; `oy` shifts them
// into the canvas. If the canvas cannot be allocated, drawing falls back to the display directly.
struct Gfx {
  lgfx::LovyanGFX* t = nullptr;
  int oy = 0;
  lgfx::LovyanGFX& d() const { return t ? *t : static_cast<lgfx::LovyanGFX&>(M5.Display); }
  template <class D> void setTextDatum(D v) { d().setTextDatum(v); }
  void setTextSize(uint8_t s) { d().setTextSize(s); }
  void setTextColor(uint16_t c) { d().setTextColor(c); }
  void drawString(const char* s, int x, int y) { d().drawString(s, x, y - oy); }
  void fillRoundRect(int x, int y, int w, int h, int r, uint16_t c) { d().fillRoundRect(x, y - oy, w, h, r, c); }
  void drawRoundRect(int x, int y, int w, int h, int r, uint16_t c) { d().drawRoundRect(x, y - oy, w, h, r, c); }
  void drawFastHLine(int x, int y, int w, uint16_t c) { d().drawFastHLine(x, y - oy, w, c); }
  void drawLine(int x0, int y0, int x1, int y1, uint16_t c) { d().drawLine(x0, y0 - oy, x1, y1 - oy, c); }
  void fillRect(int x, int y, int w, int h, uint16_t c) { d().fillRect(x, y - oy, w, h, c); }
  void drawFastVLine(int x, int y, int h, uint16_t c) { d().drawFastVLine(x, y - oy, h, c); }
  void pushImage(int x, int y, int w, int h, const uint16_t* p) { d().pushImage(x, y - oy, w, h, p); }
};
Gfx g;
// Two sprites, as in the ACARS dashboard: a frame is composed in g_canvas and compared with g_front (what the
// panel shows). Only differing tiles are pushed, because a whole-frame push is a slow strided write on the rotated
// panel and shows as a right-to-left sweep. A page change draws straight to the display with the native
// primitives; the composed sprite then becomes the reference for later partial pushes.
M5Canvas* g_canvas = nullptr;
M5Canvas* g_front = nullptr;
bool g_front_valid = false;
constexpr int kBodyHeight = 522;  // kBodyBottom - kBodyY + 1
constexpr size_t kMaxPushRects = 96;
constexpr int kWholeFramePercent = 55;

M5Canvas* make_canvas() {
  M5Canvas* c = new (std::nothrow) M5Canvas(&M5.Display);
  if (!c) return nullptr;
  c->setColorDepth(16);
  c->setPsram(true);
  if (!c->createSprite(1280, kBodyHeight)) { delete c; return nullptr; }
  return c;
}
M5Canvas* body_canvas() {
  if (g_canvas) return g_canvas;
  g_canvas = make_canvas();
  if (g_canvas && !g_front) g_front = make_canvas();  // optional: without it every refresh draws directly
  g_front_valid = false;
  return g_canvas;
}
void release_canvas() {
  for (M5Canvas** c : {&g_canvas, &g_front}) {
    if (*c) { (*c)->deleteSprite(); delete *c; *c = nullptr; }
  }
  g_front_valid = false;
}

bool hit(int x, int y, int rx, int ry, int rw, int rh) {
  return x >= rx && y >= ry && x < rx + rw && y < ry + rh;
}
void label(const char* value, int x, int y, uint16_t color = TFT_WHITE, uint8_t scale = 2) {
  g.setTextDatum(middle_left);
  g.setTextSize(scale);
  g.setTextColor(color);
  g.drawString(value, x, y);
}
void box(int x, int y, int w, int h, uint16_t border = kEdge) {
  g.fillRoundRect(x, y, w, h, 8, kPanel);
  g.drawRoundRect(x, y, w, h, 8, border);
}
void heading(const char* text, int x, int y) { label(text, x, y, kCyan, 2); }
void divider(int x, int y, int width) { g.drawFastHLine(x, y, width, kEdge); }
// ---- Ectoplasm spectrum and waterfall --------------------------------------------------------------------------
// Colour is only a map over measured data (see dead_air_palette.hpp). The waterfall is a wrapping buffer: each new
// spectrum frame overwrites the oldest row and a bright head line marks the write position, so only one row changes
// per frame. That keeps the push small on the rotated panel, where scrolling a whole image would be too slow.
constexpr int kPlotX = 373, kPlotY = 166, kPlotW = 862, kPlotH = 130;
constexpr int kWfX = 373, kWfY = 306, kWfW = 862, kWfH = 150;
constexpr int kLegendY = 464;
constexpr float kTrailDecayDb = 1.5f;  // per spectrum frame
struct Viz {
  uint16_t lut[viz::kLutSize];
  uint16_t* image = nullptr;  // kWfW x kWfH, byte-swapped RGB565 (the panel's pushImage order)
  viz::Trail trail;
  int head = 0;
  bool have_frame = false;
  bool last_valid = false;
  uint32_t last_seq = 0, last_freq = 0;
};
Viz* g_viz = nullptr;
uint16_t swap16(uint16_t c) { return static_cast<uint16_t>((c >> 8) | (c << 8)); }

void viz_alloc() {
  if (g_viz) return;
  g_viz = new (std::nothrow) Viz();
  if (!g_viz) return;
  viz::build_lut(g_viz->lut);
  g_viz->image = new (std::nothrow) uint16_t[static_cast<size_t>(kWfW) * kWfH];
  if (g_viz->image) {
    const uint16_t dark = swap16(g_viz->lut[0]);
    for (size_t i = 0; i < static_cast<size_t>(kWfW) * kWfH; ++i) g_viz->image[i] = dark;
  }
}
void viz_release() {
  if (!g_viz) return;
  delete[] g_viz->image;
  delete g_viz;
  g_viz = nullptr;
}
void viz_write_row(const float* bins) {
  uint16_t* row = g_viz->image + static_cast<size_t>(g_viz->head) * kWfW;
  if (bins) viz::render_row(bins, kBins, g_viz->lut, row, kWfW);
  else viz::render_gap_row(row, kWfW);
  for (int x = 0; x < kWfW; ++x) row[x] = swap16(row[x]);
  g_viz->head = (g_viz->head + 1) % kWfH;
}
// Called once per snapshot. Rows are only written from a real, settled spectrum frame; a retune (including every sweep
// hop) or a break in valid data leaves a marker row so the picture never implies continuity that did not exist.
void viz_feed(const Snapshot& s) {
  if (!g_viz || !g_viz->image) return;
  const bool valid = s.receiver_ready && s.spectrum_valid;
  if (!valid) { g_viz->last_valid = false; return; }
  if (g_viz->have_frame && (!g_viz->last_valid || s.frequency_hz != g_viz->last_freq)) {
    viz_write_row(nullptr);
    g_viz->trail.reset();
  }
  g_viz->last_valid = true;
  g_viz->last_freq = s.frequency_hz;
  if (!g_viz->have_frame || s.spectrum_seq != g_viz->last_seq) {
    viz_write_row(s.spectrum_dbfs);
    g_viz->trail.update(s.spectrum_dbfs, kBins, kTrailDecayDb);
    g_viz->last_seq = s.spectrum_seq;
    g_viz->have_frame = true;
  }
}

int level_y(float dbfs, int top, int height) {
  return top + height - 1 - static_cast<int>(viz::normalize(dbfs) * static_cast<float>(height - 2));
}
void draw_trace() {
  const uint16_t bg = viz::rgb565(6, 3, 18), grid = viz::rgb565(34, 18, 66);
  g.fillRect(kPlotX, kPlotY, kPlotW, kPlotH, bg);
  for (int db = -20; db >= -100; db -= 20) {
    g.drawFastHLine(kPlotX, level_y(static_cast<float>(db), kPlotY, kPlotH), kPlotW, grid);
  }
  const bool live = g_snapshot->receiver_ready && g_snapshot->spectrum_valid;
  if (live && g_viz) {
    const uint16_t* lut = g_viz->lut;
    // Every pixel's colour depends only on the level its own row stands for, never on the column's trace height, so
    // between frames only the pixels near the moving edge change and the tile push stays small.
    uint16_t body[kPlotH], edge[kPlotH], bright[kPlotH], halo[kPlotH];
    for (int row = 0; row < kPlotH; ++row) {
      const float t = std::clamp(static_cast<float>(kPlotH - 2 - row) / static_cast<float>(kPlotH - 2), 0.0f, 1.0f);
      body[row] = lut[static_cast<int>(t * 70.0f)];
      edge[row] = lut[static_cast<int>(t * 150.0f)];
      bright[row] = lut[std::min(255, 190 + static_cast<int>(t * 65.0f))];
      halo[row] = lut[static_cast<int>(t * 110.0f)];
    }
    static_assert(kPlotW <= 1024, "column scratch is sized for the plot width");
    int top[kPlotW];
    for (int x = 0; x < kPlotW; ++x) {
      top[x] = level_y(viz::interpolate_bins(g_snapshot->spectrum_dbfs, kBins, x, kPlotW), kPlotY, kPlotH) - kPlotY;
    }
    if (g_viz->trail.valid()) {  // fading afterimage (peak hold), behind the live trace
      float trail_bins[viz::Trail::kMax];
      const size_t n = std::min<size_t>(g_viz->trail.count(), viz::Trail::kMax);
      for (size_t i = 0; i < n; ++i) trail_bins[i] = g_viz->trail.at(i);
      for (int x = 0; x < kPlotW; ++x) {
        const float v = viz::interpolate_bins(trail_bins, n, x, kPlotW);
        const int row = level_y(v, kPlotY, kPlotH) - kPlotY;
        g.drawFastVLine(kPlotX + x, kPlotY + row, 1, viz::color_at(lut, v - 8.0f));
      }
    }
    // Row by row: runs of columns in the same zone become one horizontal line.
    for (int row = 0; row < kPlotH; ++row) {
      int run_start = 0, run_zone = -1;
      for (int x = 0; x <= kPlotW; ++x) {
        int zone = -1;  // -1 above the trace, 0 halo, 1 bright edge, 2 glow, 3 body
        if (x < kPlotW) {
          const int d = row - top[x];
          zone = d < -1 ? -1 : d == -1 ? 0 : d == 0 ? 1 : d < 4 ? 2 : 3;
        }
        if (zone != run_zone) {
          if (run_zone >= 0 && x > run_start) {
            const uint16_t c = run_zone == 0 ? halo[row] : run_zone == 1 ? bright[row] : run_zone == 2 ? edge[row] : body[row];
            g.drawFastHLine(kPlotX + run_start, kPlotY + row, x - run_start, c);
          }
          run_start = x;
          run_zone = zone;
        }
      }
    }
  } else {
    label("NO LIVE SPECTRUM", kPlotX + 16, kPlotY + kPlotH / 2, kMuted, 2);
  }
  g.drawRoundRect(kPlotX, kPlotY, kPlotW, kPlotH, 0, grid);
  label("-20", kPlotX + 6, level_y(-20.0f, kPlotY, kPlotH) + 10, kMuted, 2);
  label("-60", kPlotX + 6, level_y(-60.0f, kPlotY, kPlotH) + 10, kMuted, 2);
  label("-100", kPlotX + 6, level_y(-100.0f, kPlotY, kPlotH) - 10, kMuted, 2);
}
void draw_waterfall() {
  const uint16_t edge = viz::rgb565(34, 18, 66);
  if (!g_viz || !g_viz->image) {
    g.fillRect(kWfX, kWfY, kWfW, kWfH, viz::rgb565(6, 3, 18));
    label("WATERFALL UNAVAILABLE (MEMORY)", kWfX + 16, kWfY + kWfH / 2, kYellow, 2);
  } else {
    g.pushImage(kWfX, kWfY, kWfW, kWfH, g_viz->image);
    // Head line = next row to be overwritten; the rows just above it are the newest.
    g.drawFastHLine(kWfX, kWfY + g_viz->head, kWfW, viz::rgb565(220, 235, 255));
    if (!g_viz->have_frame) label("WAITING FOR A MEASURED SPECTRUM", kWfX + 16, kWfY + kWfH / 2, kMuted, 2);
  }
  g.drawRoundRect(kWfX, kWfY, kWfW, kWfH, 0, edge);
}
void draw_legend() {
  if (g_viz) {
    for (int x = 0; x < kWfW; ++x)
      g.drawFastVLine(kWfX + x, kLegendY, 10, g_viz->lut[x * (viz::kLutSize - 1) / (kWfW - 1)]);
  }
  static const char* const kTicks[] = {"-110", "-85", "-60", "-35", "-10 dBFS"};
  for (int i = 0; i < 5; ++i) {
    const int len = static_cast<int>(std::strlen(kTicks[i]));
    int x = kWfX + i * (kWfW - 1) / 4 - len * 6;
    x = std::max(kWfX, std::min(x, kWfX + kWfW - len * 12));
    label(kTicks[i], x, 489, kMuted, 2);
  }
}

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
  if (state == Status::stale) label("STALE", x + 400, y, kYellow, 2);
}
void plot(const float* data, size_t count, bool valid, int x, int y, int w, int h,
          float minimum, float maximum, uint16_t color) {
  g.drawRoundRect(x, y, w, h, 0, kEdge);
  for (int t = 1; t < 4; ++t)
    g.drawFastHLine(x + 1, y + h * t / 4, w - 2, kEdge);
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
    g.drawLine(xa, py(data[i - 1]), xb, py(data[i]), color);
  }
}
const char* recorder_text(RecorderState state) {
  switch (state) {
    case RecorderState::starting: return "STARTING";
    case RecorderState::active: return "SAVING TO SD";
    case RecorderState::failed: return "SD FAILED";
    case RecorderState::stopped: return "STOPPED";
    default: return "NO SD / OFF";
  }
}
uint16_t recorder_color(RecorderState state) {
  switch (state) {
    case RecorderState::active: return kGreen;
    case RecorderState::starting: return kYellow;
    case RecorderState::failed: return kRed;
    default: return kMuted;
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
void sanitize_snapshot() {
  // A disconnected module may leave cached readings. Never render those live.
  if (!g_snapshot->environment_sensor_attached) {
    g_snapshot->temperature_c = {};
    g_snapshot->humidity_rh = {};
    g_snapshot->pressure_hpa = {};
  }
  if (!g_snapshot->receiver_ready) {
    g_snapshot->rf_level_dbfs = {};
    g_snapshot->spectrum_valid = false;
  }
  if (!g_snapshot->audio_available) {
    g_snapshot->audio_level_dbfs = {};
    g_snapshot->audio_envelope_valid = false;
  }
}
void chrome() {
  M5.Display.setFont(nullptr);
  M5.Display.fillScreen(kBg);
  audio_header::draw_brand("DEAD AIR");
  audio_header::draw_battery(g_snapshot->battery_percent);
  audio_header::draw_home_button();
  audio_header::draw_mute_button(g_snapshot->sound_enabled);
  audio_header::draw_visualizer_button(false);
  audio_header::draw_settings_button();
  nav();
}
void overview() {
  box(20, 120, 612, 252);
  heading("INVESTIGATION", 40, 143);
  label(g_snapshot->recording ? "RECORDING" : "READY", 40, 190,
        g_snapshot->recording ? kRed : kGreen, 3);
  char buffer[52];
  snprintf(buffer, sizeof(buffer), "ELAPSED %02lu:%02lu:%02lu",
    static_cast<unsigned long>(g_snapshot->session_elapsed_s / 3600),
    static_cast<unsigned long>(g_snapshot->session_elapsed_s / 60 % 60),
    static_cast<unsigned long>(g_snapshot->session_elapsed_s % 60));
  label(buffer, 40, 240, TFT_WHITE);
  if (g_snapshot->receiver_ready) {
    snprintf(buffer, sizeof(buffer), "%.3f MHz  %s", g_snapshot->frequency_hz / 1e6,
             g_snapshot->demod);
    label(buffer, 40, 291, kCyan, 3);
  } else label("RECEIVER UNAVAILABLE", 40, 291, kYellow, 2);
  char journal[64];
  snprintf(journal, sizeof(journal), "EVENT LOG: %s", recorder_text(g_snapshot->recorder_state));
  label(journal, 40, 343, recorder_color(g_snapshot->recorder_state), 2);
  box(646, 120, 614, 252);
  heading("LIVE OBSERVATIONS", 668, 143);
  reading("RF (dBFS)", g_snapshot->rf_level_dbfs, 670, 192, "%.1f");
  reading("AUDIO (dBFS)", g_snapshot->audio_level_dbfs, 670, 247, "%.1f");
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
    label(row, 40, 462 + i * 38, i == 0 ? kGreen : TFT_WHITE, 2);
  }
  box(646, 390, 614, 241);
  heading("ENV III / OPTIONAL", 668, 413);
  label(g_snapshot->environment_sensor_attached ? "MODULE DETECTED" : "MODULE NOT CONNECTED",
        668, 464, g_snapshot->environment_sensor_attached ? kGreen : kMuted, 2);
  reading("TEMP (C)", g_snapshot->temperature_c, 668, 505, "%.1f");
  reading("RH (%)", g_snapshot->humidity_rh, 668, 550, "%.1f");
  reading("PRESSURE (hPa)", g_snapshot->pressure_hpa, 668, 594, "%.1f");
}
void spectrum() {
  box(20, 120, 312, 511);
  heading("TUNING", 40, 144);
  char hz[48];
  if (g_snapshot->receiver_ready) snprintf(hz, sizeof(hz), "%.3f MHz", g_snapshot->frequency_hz / 1e6);
  else snprintf(hz, sizeof(hz), "NO RECEIVER");
  label(hz, 40, 201, kGreen, 3);
  divider(40, 248, 271);
  label("MODE (RECEIVER DEMODULATOR)", 40, 270, kMuted, 2);
  for (int i = 0; i < kModeCount; ++i) {
    const bool selected = g_snapshot->receiver_ready && std::strcmp(g_snapshot->demod, kModeNames[i]) == 0;
    const int x = kModeX0 + (i % 3) * kModeDx, y = kModeY0 + (i / 3) * kModeDy;
    box(x, y, kModeW, 52, selected ? kGreen : g_snapshot->receiver_ready ? kCyan : kEdge);
    label(kModeNames[i], x + 14, y + 26, selected ? kGreen : g_snapshot->receiver_ready ? TFT_WHITE : kMuted);
  }
  label("TUNE (BAND STEP)", 40, 432, kMuted, 2);
  button("- STEP", 40, 452, 128, kCyan, g_snapshot->receiver_ready && !g_snapshot->sweeping);
  button("+ STEP", 183, 452, 128, kCyan, g_snapshot->receiver_ready && !g_snapshot->sweeping);
  label(g_snapshot->receiver_settled ? "SETTLED" : "SETTLING...", 40, 535,
        g_snapshot->receiver_settled ? kGreen : kYellow, 2);
  label("Single channel,", 40, 580, kMuted, 2);
  label("not a sweep", 40, 605, kMuted, 2);
  box(346, 120, 914, 511);
  heading("RF SPECTRUM  /  dBFS", 366, 144);
  if (g_snapshot->receiver_ready) label(hz, 986, 144, kGreen, 2);
  draw_trace();
  draw_waterfall();
  draw_legend();
  heading("SWEEP  /  GENERATED SCHEDULE", 366, 509);
  const bool ready = g_snapshot->receiver_ready;
  const bool run = g_snapshot->sweeping;
  button(run ? "STOP SWEEP" : "START SWEEP", 366, kSweepY, 230, run ? kRed : kGreen, ready);
  button(sweep_preset(g_snapshot->sweep_preset).name, 606, kSweepY, 300, kCyan, ready && !run);
  char dwell[32];
  snprintf(dwell, sizeof(dwell), "DWELL %u ms", static_cast<unsigned>(g_snapshot->sweep_dwell_ms));
  button(dwell, 916, kSweepY, 320, kCyan, ready && !run);
  char info[96];
  if (run)
    snprintf(info, sizeof(info), "HOP %lu  UNOBSERVED %.1f s  RF DETECTOR OFF",
             static_cast<unsigned long>(g_snapshot->sweep_hops), g_snapshot->sweep_unobserved_ms / 1000.0);
  else
    snprintf(info, sizeof(info), "One tuner, one channel at a time. Not simultaneous coverage.");
  label(info, 366, 608, run ? kYellow : kMuted, 2);
}
void audio() {
  box(20, 120, 1240, 251);
  heading("DEMODULATED AUDIO  /  NORMALIZED ENVELOPE", 40, 143);
  plot(g_snapshot->audio_envelope, kBins, g_snapshot->audio_available &&
       g_snapshot->audio_envelope_valid, 42, 171, 1194, 180, -1.0f, 1.0f, kCyan);
  box(20, 388, 773, 243);
  heading("AUDIO ANALYSIS", 40, 415);
  reading("LEVEL (dBFS)", g_snapshot->audio_level_dbfs, 40, 485, "%.1f");
  label("No automatic speech claims", 40, 560, kMuted);
  box(809, 388, 451, 243);
  heading("SOURCE", 829, 415);
  label(g_snapshot->receiver_ready ? g_snapshot->demod : "NO AUDIO SOURCE", 829, 482, TFT_WHITE, 2);
  label("Clip replay: not implemented", 829, 551, kMuted, 2);
}
void environment() {
  box(20, 120, 610, 511);
  heading("ENV III / OPTIONAL SENSOR", 40, 144);
  label(g_snapshot->environment_sensor_attached ? "CONNECTED" : "UNAVAILABLE",
        40, 208, g_snapshot->environment_sensor_attached ? kGreen : kMuted, 3);
  reading("TEMPERATURE (C)", g_snapshot->temperature_c, 40, 280, "%.1f");
  reading("HUMIDITY (%)", g_snapshot->humidity_rh, 40, 355, "%.1f");
  reading("PRESSURE (hPa)", g_snapshot->pressure_hpa, 40, 430, "%.1f");
  label("Core RF investigation works without ENV III.", 40, 548, kMuted, 2);
  box(644, 120, 616, 511);
  heading("SENSOR INTEGRITY", 666, 144);
  label("No derived score without validated baselines", 666, 201, kMuted, 2);
  reading("DEVICE MOTION (g)", g_snapshot->motion_g, 666, 271, "%.3f");
  label("This describes Tab5 motion, NOT room movement.", 666, 331, kMuted, 2);
  label("Temperature / humidity / pressure", 666, 426, kCyan, 2);
  label("shown only from current valid sensor samples", 666, 476, kMuted, 2);
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
  label("EVENT LOG (JSONL)", 40, 200, kMuted, 2);
  label(recorder_text(g_snapshot->recorder_state), 40, 245,
        recorder_color(g_snapshot->recorder_state), 3);
  char line[56];
  if (g_snapshot->recorder_session) {
    snprintf(line, sizeof(line), "Session %lu", static_cast<unsigned long>(g_snapshot->recorder_session));
    label(line, 40, 300, TFT_WHITE, 2);
  }
  snprintf(line, sizeof(line), "Saved %lu", static_cast<unsigned long>(g_snapshot->recorder_saved));
  label(line, 40, 345, TFT_WHITE, 2);
  snprintf(line, sizeof(line), "Lost %lu  Dropped %lu",
           static_cast<unsigned long>(g_snapshot->recorder_missing),
           static_cast<unsigned long>(g_snapshot->recorder_dropped));
  label(line, 40, 390,
        g_snapshot->recorder_missing || g_snapshot->recorder_dropped ? kYellow : kMuted, 2);
  label("Audio/IQ clips: not implemented", 40, 560, kMuted, 2);
  box(497, 120, 763, 511);
  heading("EVIDENCE REVIEW", 517, 144);
  plot(nullptr, 0, false, 519, 195, 718, 248, -1.0f, 1.0f, kGreen);
  label("WAV / IQ clip indexing and hashes", 519, 498, kMuted);
  label("will appear after the storage adapter is wired.", 519, 550, kMuted);
}
void paint_page() {
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
void paint_direct() {
  g.t = &M5.Display;
  g.oy = 0;
  M5.Display.fillRect(0, kBodyY, 1280, kBodyBottom - kBodyY + 1, kBg);
  paint_page();
}
// page_change: another screen is replacing this one, so most pixels differ and tile pushes would not help.
uint32_t now_us() {
#if defined(ESP_PLATFORM)
  return static_cast<uint32_t>(esp_timer_get_time());
#else
  return static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::microseconds>(
                                   std::chrono::steady_clock::now().time_since_epoch()).count());
#endif
}
PaintStats g_stats{};

void draw_body(bool page_change = false) {
  M5Canvas* canvas = body_canvas();
  if (!canvas) { paint_direct(); return; }
  const uint32_t t0 = now_us();
  g.t = canvas;
  g.oy = kBodyY;
  canvas->fillSprite(kBg);
  paint_page();
  g.t = &M5.Display;
  g.oy = 0;
  const uint32_t t1 = now_us();
  acars_ui::DirtyRect rects[kMaxPushRects];
  size_t count = 0;
  bool whole = true;
  if (!page_change && g_front && g_front_valid) {
    const uint16_t* next = static_cast<const uint16_t*>(canvas->getBuffer());
    const uint16_t* previous = static_cast<const uint16_t*>(g_front->getBuffer());
    if (next && previous) {
      bool overflow = false;
      count = acars_ui::diff_rects(previous, next, 1280, kBodyHeight, rects, kMaxPushRects, &overflow);
      const int64_t area = acars_ui::rect_area(rects, count);
      whole = overflow || area * 100 > static_cast<int64_t>(1280) * kBodyHeight * kWholeFramePercent;
      g_stats.pixels = static_cast<uint32_t>(area);
    }
  }
  const uint32_t t2 = now_us();
  if (whole) {
    paint_direct();  // native primitives straight to the panel, like the other dashboards
    ++g_stats.whole_pushes;
    g_stats.pixels = 1280u * kBodyHeight;
  } else {
    for (size_t i = 0; i < count; ++i) {
      M5.Display.setClipRect(rects[i].x, kBodyY + rects[i].y, rects[i].w, rects[i].h);
      canvas->pushSprite(0, kBodyY);
    }
    M5.Display.clearClipRect();
    ++g_stats.partial_pushes;
  }
  const uint32_t t3 = now_us();
  g_stats.compose_us = t1 - t0;
  g_stats.diff_us = t2 - t1;
  g_stats.push_us = t3 - t2;
  g_stats.rects = whole ? 1u : static_cast<uint32_t>(count);
  g_stats.push_us_max = std::max(g_stats.push_us_max, g_stats.push_us);
  if (g_front) {
    std::swap(g_canvas, g_front);  // the frame now on the panel becomes the reference
    g_front_valid = true;
  }
}
} // namespace

void attach_events(EventModel* events) { g_events = events; }
void enter(const Snapshot& snapshot) {
  if (!screens::is_active(screens::Id::dead_air)) return;
  if (!g_snapshot) g_snapshot = new (std::nothrow) Snapshot();
  if (!g_snapshot) return;
  *g_snapshot = snapshot;
  sanitize_snapshot();
  g_page = Page::overview;
  g_active = true;
  viz_alloc();
  viz_feed(*g_snapshot);
  chrome();
  draw_body(true);
}
void update(const Snapshot& snapshot) {
  if (!g_active || !screens::owns(screens::Id::dead_air) ||
      snapshot.revision == g_snapshot->revision) return;
  const int battery = g_snapshot->battery_percent;
  const bool sound = g_snapshot->sound_enabled;
  *g_snapshot = snapshot;
  sanitize_snapshot();
  viz_feed(*g_snapshot);
  if (sound != snapshot.sound_enabled) audio_header::draw_mute_button(snapshot.sound_enabled);
  if (battery != snapshot.battery_percent) audio_header::draw_battery(snapshot.battery_percent);
  draw_body();
}
void leave() {
  g_active = false;
  release_canvas();
  viz_release();
}
bool active() { return g_active; }
PaintStats paint_stats() { return g_stats; }
Page page() { return g_page; }
Action handle_touch(int32_t x, int32_t y, uint32_t now_ms) {
  if (!g_active || !screens::owns(screens::Id::dead_air)) return {};
  if (audio_header::home_hit(x, y)) return {ActionKind::close};
  if (audio_header::mute_hit(x, y)) return {ActionKind::toggle_mute};
  if (audio_header::settings_hit(x, y)) return {ActionKind::open_settings};
  if (y >= kTabY && y < kTabY + kTabHeight) {
    for (uint8_t i = 0; i < static_cast<uint8_t>(Page::count); ++i) {
      if (hit(x, y, kTabX + i * (kTabWidth + kTabGap), kTabY, kTabWidth, kTabHeight)) {
        if (g_page != static_cast<Page>(i)) {
          g_page = static_cast<Page>(i);
          nav(); draw_body(true);
        }
        return {};
      }
    }
  }
  if (g_page == Page::spectrum && g_snapshot->receiver_ready) {
    if (hit(x, y, 366, kSweepY, 230, 52))
      return {g_snapshot->sweeping ? ActionKind::stop_scan : ActionKind::start_scan, 0};
    if (!g_snapshot->sweeping) {
      if (hit(x, y, 606, kSweepY, 300, 52)) return {ActionKind::sweep_preset, 0};
      if (hit(x, y, 916, kSweepY, 320, 52)) return {ActionKind::sweep_dwell, 0};
      if (hit(x, y, 40, kStepY, 128, 52)) return {ActionKind::tune_down, -1};
      if (hit(x, y, 183, kStepY, 128, 52)) return {ActionKind::tune_up, 1};
      for (int i = 0; i < kModeCount; ++i)
        if (hit(x, y, kModeX0 + (i % 3) * kModeDx, kModeY0 + (i / 3) * kModeDy, kModeW, 52))
          return {ActionKind::set_demod, i};
    }
  }
  if (g_page == Page::events && g_events != nullptr) {
    if (hit(x, y, 818, 505, 420, 52)) {
      g_events->mark_manual(now_ms, g_snapshot->unix_time, g_snapshot->frequency_hz);
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
