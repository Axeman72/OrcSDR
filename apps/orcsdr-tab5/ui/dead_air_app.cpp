#include "dead_air_app.hpp"

#include <M5Unified.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <new>

#include "dead_air_dashboard.hpp"
#include "dead_air_journal.hpp"
#include "dead_air_model.hpp"
#include "dead_air_sd_journal.hpp"
#include "dead_air_sweep.hpp"
#include "orcsdr_storage.hpp"
#include "screen_controller.hpp"

namespace orcsdr::dead_air_app {
namespace {
using namespace orcsdr::dead_air;

constexpr uint32_t kObserveIntervalMs = 50;     // detector cadence: persistence is 200 ms
constexpr uint32_t kSettleMs = 800;             // after a retune/gain/rate change or receiver (re)start
constexpr uint32_t kSnapshotIntervalMs = 250;   // slowest visible refresh when nothing else changed
constexpr uint32_t kSpectrumFreshMs = 1000;
constexpr uint32_t kWorkerStopWaitMs = 1500;
// Full scale matches the receiver's IQ meter (rtl_signal_dbfs): a CU8 complex sample with both axes
// at the rail, |x| = 127.5 * sqrt(2). Bin values are per-bin power of a Hann-windowed 2048 point FFT.
constexpr float kFullScaleAmplitude = 127.5f * 1.41421356f;

struct Worker {
  std::atomic<bool> stop{false};
  std::atomic<bool> alive{false};
  std::atomic<uint8_t> state{static_cast<uint8_t>(RecorderState::unavailable)};
  std::atomic<uint32_t> session{0};
  std::atomic<uint32_t> saved{0};
  std::atomic<uint32_t> missing{0};
  std::atomic<uint32_t> dropped{0};
  TaskHandle_t task = nullptr;
};

struct State {
  EventModel model;
  EventQueue queue;
  Worker worker;
  SdJournalSink sink;
  EventJournal journal;
  Snapshot snapshot;
  float spectrum[kBins]{};
  uint32_t spectrum_ms = 0;
  bool spectrum_have = false;
  uint32_t spectrum_seq = 0;
  uint32_t opened_ms = 0;
  uint32_t last_observe_ms = 0, last_snapshot_ms = 0, settle_until_ms = 0;
  uint32_t last_signature = 0, config_revision = 0, last_counter = 0;
  bool have_signature = false;
  float last_level = 0.0f;
  bool last_level_valid = false;
  uint32_t last_total = 0;
  Sweep sweep;
  uint8_t sweep_preset = 0;
  uint16_t sweep_dwell_ms = 500;
};

Hooks g_hooks;
State* g_state = nullptr;
bool g_open = false;
uint8_t g_last_action = 0;     // diagnostic: last touch action kind seen by the app
uint32_t g_touches = 0;

bool ready_hooks() {
  return g_hooks.receiver_ready && g_hooks.frequency_hz && g_hooks.demod && g_hooks.sample_rate &&
         g_hooks.config_signature && g_hooks.rf_sample_counter && g_hooks.rf_level_dbfs;
}

void set_state(Worker& w, RecorderState s) { w.state.store(static_cast<uint8_t>(s), std::memory_order_release); }

// Dedicated, low-priority storage worker. It owns the SD sink and journal; the UI task only
// pushes events into the lock-free queue and reads the health atomics.
void worker_main(void* arg) {
  State* s = static_cast<State*>(arg);
  Worker& w = s->worker;
  bool open = false;
  set_state(w, RecorderState::starting);
  if (g_hooks.sd_ready && g_hooks.sd_ready()) {
    const uint32_t utc = g_hooks.utc_epoch ? g_hooks.utc_epoch() : 0;
    // Unique even without a wall clock: SdJournalSink::open skips existing files.
    if (s->sink.open(storage::filesystem(), utc ? utc : 1) && s->journal.start(s->sink, s->sink.session_id())) {
      open = true;
      w.session.store(s->sink.session_id(), std::memory_order_release);
      set_state(w, RecorderState::active);
    }
  }
  if (!open) {
    set_state(w, RecorderState::failed);
    if (s->sink.opened()) (void)s->sink.close();
  }
  while (open && !w.stop.load(std::memory_order_acquire)) {
    s->journal.drain(s->queue, s->sink, 8);
    w.saved.store(s->journal.saved(), std::memory_order_relaxed);
    w.missing.store(s->journal.missing(), std::memory_order_relaxed);
    w.dropped.store(s->queue.dropped(), std::memory_order_relaxed);
    if (s->journal.failed() || s->sink.failed()) {
      set_state(w, RecorderState::failed);
      open = false;
      break;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
  if (open) {
    const bool clean = s->journal.finish(s->queue, s->sink);
    const bool closed = s->sink.close();
    w.saved.store(s->journal.saved(), std::memory_order_relaxed);
    w.missing.store(s->journal.missing(), std::memory_order_relaxed);
    w.dropped.store(s->queue.dropped(), std::memory_order_relaxed);
    set_state(w, clean && closed ? RecorderState::stopped : RecorderState::failed);
  } else if (s->sink.opened()) {
    (void)s->sink.close();
  }
  // The final state (stopped/failed) stays published for the dashboard.
  w.alive.store(false, std::memory_order_release);
  w.task = nullptr;
  vTaskDeleteWithCaps(nullptr);
}

void log_sweep(State& s, Kind kind, uint32_t now) {
  Event e{};
  e.monotonic_ms = now;
  e.unix_time = g_hooks.utc_epoch ? g_hooks.utc_epoch() : 0;
  e.source = Source::sweep;
  e.kind = kind;
  const SweepConfig& c = s.sweep.config();
  e.frequency_hz = kind == Kind::sweep_start ? c.start_hz : s.sweep.current_hz();
  e.sweep_end_hz = c.end_hz;
  e.sweep_step_hz = c.step_hz;
  e.sweep_dwell_ms = c.dwell_ms;
  e.sweep_settle_ms = kSweepSettleMs;
  e.sweep_hops = s.sweep.hops();
  e.sweep_unobserved_ms = s.sweep.unobserved_ms();
  s.model.mark_sweep(e);
}

void stop_sweep(State& s, uint32_t now) {
  if (!s.sweep.running()) return;
  s.sweep.stop();
  log_sweep(s, Kind::sweep_stop, now);
  // Leave the receiver where it was before the sweep started.
  if (s.sweep.resume_hz()) {
    if (!(g_hooks.hot_retune && g_hooks.hot_retune(s.sweep.resume_hz())) && g_hooks.tune) g_hooks.tune(s.sweep.resume_hz());
  }
  s.settle_until_ms = now + kSettleMs;
}

bool start_sweep(State& s, uint32_t now) {
  if (s.sweep.running() || !g_hooks.receiver_ready()) return false;
  SweepConfig c = sweep_preset(s.sweep_preset).config;
  c.dwell_ms = s.sweep_dwell_ms;
  if (!s.sweep.start(c, g_hooks.frequency_hz(), now)) return false;
  log_sweep(s, Kind::sweep_start, now);
  return true;
}

bool ensure_state() {
  if (g_state) return true;
  void* memory = heap_caps_calloc(1, sizeof(State), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!memory) return false;
  g_state = new (memory) State();
  return true;
}

bool start_worker() {
  Worker& w = g_state->worker;
  const uint32_t begin = millis();
  while (w.alive.load(std::memory_order_acquire) && millis() - begin < kWorkerStopWaitMs) vTaskDelay(pdMS_TO_TICKS(10));
  if (w.alive.load(std::memory_order_acquire)) return false;  // previous worker still closing its file
  w.stop.store(false, std::memory_order_release);
  w.saved = w.missing = w.dropped = 0;
  w.session = 0;
  g_state->sink = SdJournalSink();
  g_state->journal = EventJournal();
  g_state->queue.~EventQueue();  // atomics are not assignable; no worker is alive, so rebuild in place
  new (&g_state->queue) EventQueue();
  w.alive.store(true, std::memory_order_release);
  set_state(w, RecorderState::starting);
  if (xTaskCreatePinnedToCoreWithCaps(worker_main, "dead_air_sd", 8192, g_state, 1, &w.task, 0,
                                      MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) != pdPASS) {
    w.alive.store(false, std::memory_order_release);
    set_state(w, RecorderState::failed);
    return false;
  }
  return true;
}

void build_snapshot(uint32_t now) {
  State& s = *g_state;
  Snapshot& n = s.snapshot;
  const bool ready = g_hooks.receiver_ready();
  n.receiver_ready = ready;
  n.receiver_settled = ready && static_cast<int32_t>(now - s.settle_until_ms) >= 0;
  n.frequency_hz = ready ? g_hooks.frequency_hz() : 0;
  n.sample_rate_sps = ready ? g_hooks.sample_rate() : 0;
  std::snprintf(n.demod, sizeof(n.demod), "%s", ready ? g_hooks.demod() : "--");
  n.sound_enabled = g_hooks.sound_enabled ? g_hooks.sound_enabled() : true;
  n.battery_percent = g_hooks.battery_percent ? g_hooks.battery_percent() : -1;
  n.unix_time = g_hooks.utc_epoch ? g_hooks.utc_epoch() : 0;
  n.session_elapsed_s = (now - s.opened_ms) / 1000u;
  n.recording = false;     // audio/IQ recording is not implemented
  n.scanning = false;      // no sweep exists; one tuned channel
  n.audio_available = false;  // no demodulated-audio level source is connected yet
  n.audio_level_dbfs = {};
  n.audio_envelope_valid = false;
  // ENV III is not connected: leave the optional sensor absent. Nothing is simulated.
  n.environment_sensor_attached = false;
  n.temperature_c = n.humidity_rh = n.pressure_hpa = n.motion_g = {};
  Reading rf{};
  rf.present = ready;
  rf.valid = ready && n.receiver_settled && s.last_level_valid;
  rf.value = s.last_level;
  rf.age_ms = rf.valid ? now - s.last_observe_ms : 0;
  rf.baseline_valid = s.model.baseline_ready();
  rf.baseline = s.model.rf_baseline_dbfs();
  n.rf_level_dbfs = rf;
  const bool spectrum_ok = ready && n.receiver_settled && s.spectrum_have && now - s.spectrum_ms < kSpectrumFreshMs;
  n.spectrum_valid = spectrum_ok;
  n.spectrum_seq = s.spectrum_seq;
  if (spectrum_ok) std::memcpy(n.spectrum_dbfs, s.spectrum, sizeof(n.spectrum_dbfs));
  n.sweeping = s.sweep.running();
  n.sweep_preset = s.sweep_preset;
  n.sweep_dwell_ms = s.sweep_dwell_ms;
  n.sweep_hops = s.sweep.hops();
  n.sweep_unobserved_ms = s.sweep.unobserved_ms();
  n.recorder_state = static_cast<RecorderState>(s.worker.state.load(std::memory_order_acquire));
  n.recorder_session = s.worker.session.load(std::memory_order_relaxed);
  n.recorder_saved = s.worker.saved.load(std::memory_order_relaxed);
  n.recorder_missing = s.worker.missing.load(std::memory_order_relaxed);
  n.recorder_dropped = s.queue.dropped();
}

// Quantised comparison so a flickering last digit does not repaint the whole panel.
bool visibly_changed(const Snapshot& a, const Snapshot& b) {
  const auto q = [](float v) { return static_cast<int>(std::lround(v * 2.0f)); };
  return a.receiver_ready != b.receiver_ready || a.receiver_settled != b.receiver_settled ||
         a.frequency_hz != b.frequency_hz || a.sound_enabled != b.sound_enabled ||
         a.battery_percent != b.battery_percent || a.session_elapsed_s != b.session_elapsed_s ||
         a.recorder_state != b.recorder_state || a.recorder_saved != b.recorder_saved ||
         a.recorder_missing != b.recorder_missing || a.recorder_dropped != b.recorder_dropped ||
         a.recorder_session != b.recorder_session || a.rf_level_dbfs.valid != b.rf_level_dbfs.valid ||
         q(a.rf_level_dbfs.value) != q(b.rf_level_dbfs.value) ||
         a.rf_level_dbfs.baseline_valid != b.rf_level_dbfs.baseline_valid ||
         a.sweeping != b.sweeping || a.sweep_hops != b.sweep_hops || a.sweep_preset != b.sweep_preset ||
         a.sweep_dwell_ms != b.sweep_dwell_ms ||
         a.spectrum_valid != b.spectrum_valid || std::strcmp(a.demod, b.demod) != 0;
}

}  // namespace

void install(const Hooks& hooks) { g_hooks = hooks; }
bool active() { return g_open && dead_air::active(); }

void open() {
  if (!ready_hooks() || !ensure_state()) return;
  State& s = *g_state;
  const uint32_t now = millis();
  s.model.reset();
  s.model.set_listener(enqueue_event, &s.queue);
  s.opened_ms = now;
  s.last_observe_ms = s.last_snapshot_ms = 0;
  s.settle_until_ms = now + kSettleMs;
  s.have_signature = false;
  s.last_level_valid = false;
  s.spectrum_have = false;
  s.last_total = 0;
  s.snapshot = Snapshot();
  (void)start_worker();
  if (g_hooks.set_demod) g_hooks.set_demod(0);  // NFM, the browse band's own demodulator
  build_snapshot(now);
  s.snapshot.revision = 1;
  screens::begin_transition(screens::Id::dead_air, now);
  dead_air::attach_events(&s.model);
  dead_air::enter(s.snapshot);
  screens::finish_transition();
  g_open = true;
}

void close() {
  if (!g_open || !g_state) return;
  g_open = false;
  stop_sweep(*g_state, millis());  // logs the stop and puts the receiver back before the journal closes
  dead_air::leave();
  g_state->model.set_listener(nullptr, nullptr);
  if (g_hooks.release_demod) g_hooks.release_demod();
  g_state->worker.stop.store(true, std::memory_order_release);  // worker flushes, syncs and closes the file
}

void tick() {
  if (!g_open || !g_state || !dead_air::active()) return;
  State& s = *g_state;
  const uint32_t now = millis();
  const bool ready = g_hooks.receiver_ready();
  const uint32_t signature = g_hooks.config_signature();
  if (!s.have_signature || signature != s.last_signature || !ready) {
    // A retune, gain/rate change or receiver (re)start invalidates the baseline and the settling window.
    if (!s.have_signature || signature != s.last_signature || s.last_level_valid) {
      ++s.config_revision;
      s.settle_until_ms = now + (s.sweep.running() ? kSweepSettleMs : kSettleMs);
    }
    s.last_signature = signature;
    s.have_signature = true;
    s.last_level_valid = false;
  }
  if (s.sweep.running()) {
    // A hop only counts as settled once the receiver reports the commanded channel and the assumed settle has passed.
    const bool settled = ready && g_hooks.frequency_hz() == s.sweep.current_hz() &&
                         static_cast<int32_t>(now - s.settle_until_ms) >= 0;
    uint32_t hz = 0;
    if (s.sweep.tick(now, ready, settled, &hz)) {
      // A refused hop is abandoned by the sweep's own stall timer and counted there.
      if (g_hooks.hot_retune) (void)g_hooks.hot_retune(hz);
      else if (g_hooks.tune) g_hooks.tune(hz);
      s.settle_until_ms = now + kSweepSettleMs;
    }
  }
  if (now - s.last_observe_ms >= kObserveIntervalMs) {
    RfObservation obs{};
    obs.monotonic_ms = now;
    obs.unix_time = g_hooks.utc_epoch ? g_hooks.utc_epoch() : 0;
    obs.frequency_hz = ready ? g_hooks.frequency_hz() : 0;
    obs.configuration_revision = s.config_revision;
    // While a sweep runs the detector is suspended: every hop is a new channel, so there is no stable baseline.
    obs.settled = ready && !s.sweep.running() && static_cast<int32_t>(now - s.settle_until_ms) >= 0;
    const uint32_t counter = g_hooks.rf_sample_counter();
    float level = 0.0f;
    const bool fresh = counter != s.last_counter;
    s.last_counter = counter;
    obs.valid = ready && fresh && g_hooks.rf_level_dbfs(&level) && std::isfinite(level);
    obs.level_dbfs = level;
    s.last_observe_ms = now;
    if (obs.valid) { s.last_level = level; s.last_level_valid = obs.settled; }
    else if (!fresh && ready) { /* between blocks: keep the last settled level */ }
    else s.last_level_valid = false;
    s.model.observe_rf(obs);
  }
  if (!screens::owns(screens::Id::dead_air)) return;
  if (now - s.last_snapshot_ms < 100) return;
  Snapshot next = s.snapshot;
  const uint32_t revision = next.revision;
  build_snapshot(now);
  const bool events_changed = s.model.total() != s.last_total;
  const bool changed = visibly_changed(s.snapshot, next) || events_changed;
  const bool spectrum_tab = dead_air::page() == Page::spectrum && s.snapshot.spectrum_valid;
  if (changed || (spectrum_tab && now - s.last_snapshot_ms >= kSnapshotIntervalMs)) {
    s.last_total = s.model.total();
    s.snapshot.revision = revision + 1;
    s.last_snapshot_ms = now;
    screens::note_visible_update(screens::Id::dead_air);
    dead_air::update(s.snapshot);
  } else {
    s.snapshot.revision = revision;
    s.last_snapshot_ms = now;
  }
}

void draw(bool static_panel) {
  if (!g_open || !g_state) return;
  if (!static_panel && !screens::may_draw(screens::Id::dead_air)) return;
  if (static_panel) {
    build_snapshot(millis());
    dead_air::enter(g_state->snapshot);
  }
}

void touch(int32_t x, int32_t y) {
  if (!g_open || !g_state) return;
  const Action action = dead_air::handle_touch(x, y, millis());
  ++g_touches;
  if (action.kind != ActionKind::none) g_last_action = static_cast<uint8_t>(action.kind);
  switch (action.kind) {
    case ActionKind::close: if (g_hooks.go_home) g_hooks.go_home(); break;
    case ActionKind::toggle_mute: if (g_hooks.toggle_mute) g_hooks.toggle_mute(); break;
    case ActionKind::open_settings: if (g_hooks.open_settings) g_hooks.open_settings(); break;
    case ActionKind::start_scan: start_sweep(*g_state, millis()); break;
    case ActionKind::stop_scan: stop_sweep(*g_state, millis()); break;
    case ActionKind::sweep_preset:
      if (!g_state->sweep.running()) g_state->sweep_preset = (g_state->sweep_preset + 1) % kSweepPresetCount;
      break;
    case ActionKind::sweep_dwell:
      if (!g_state->sweep.running()) {
        uint8_t i = 0;
        while (i < kSweepDwellCount && kSweepDwellChoicesMs[i] != g_state->sweep_dwell_ms) ++i;
        g_state->sweep_dwell_ms = kSweepDwellChoicesMs[(i + 1) % kSweepDwellCount];
      }
      break;
    case ActionKind::set_demod:
      if (g_state->sweep.running()) break;  // the schedule owns tuning while it runs
      if (g_hooks.set_demod && g_hooks.receiver_ready() && action.value >= 0 && action.value < kModeCount) {
        g_hooks.set_demod(static_cast<uint8_t>(action.value));
        if (g_hooks.tune) g_hooks.tune(g_hooks.frequency_hz());  // restart the stream cleanly in the new mode
      }
      break;
    case ActionKind::tune_up:
    case ActionKind::tune_down:
      if (g_state->sweep.running()) break;
      if (g_hooks.tune && g_hooks.step_frequency && g_hooks.receiver_ready()) {
        g_hooks.tune(g_hooks.step_frequency(g_hooks.frequency_hz(), action.value > 0 ? 1 : -1));
      }
      break;
    default: break;  // recording and clip actions are not implemented and have no buttons
  }
}

bool wants_spectrum() {
  return active() && screens::owns(screens::Id::dead_air) && dead_air::page() == Page::spectrum;
}

void feed_spectrum(const float* levels, size_t bins, uint32_t sample_rate_sps) {
  (void)sample_rate_sps;
  if (!g_open || !g_state || !levels || bins < kBins) return;
  const float offset_db = 20.0f * std::log10(kFullScaleAmplitude * (static_cast<float>(bins) - 1.0f) * 0.5f);
  State& s = *g_state;
  for (size_t i = 0; i < kBins; ++i) {
    const size_t a = i * bins / kBins, b = std::max(a + 1, (i + 1) * bins / kBins);
    float peak = -200.0f;
    for (size_t k = a; k < b && k < bins; ++k) peak = std::max(peak, levels[k]);
    s.spectrum[i] = peak - offset_db;  // max-pooled per-bin power, relative dBFS
  }
  s.spectrum_ms = millis();
  s.spectrum_have = true;
  ++s.spectrum_seq;
}

void describe(char* out, size_t size) {
  if (!out || !size) return;
  if (!g_state) { std::snprintf(out, size, "DEAD_AIR open=0"); return; }
  const Worker& w = g_state->worker;
  std::snprintf(out, size,
                "DEAD_AIR open=%d recorder=%u session=%lu saved=%lu missing=%lu dropped=%lu events=%lu baseline=%d level=%.1f base_dbfs=%.1f thr_db=%.0f hz=%lu mode=%s sweep=%d hops=%lu stalls=%lu unobs_ms=%lu touches=%lu last_action=%u resume=%lu",
                g_open ? 1 : 0, static_cast<unsigned>(w.state.load()), static_cast<unsigned long>(w.session.load()),
                static_cast<unsigned long>(w.saved.load()), static_cast<unsigned long>(w.missing.load()),
                static_cast<unsigned long>(w.dropped.load()), static_cast<unsigned long>(g_state->model.total()),
                g_state->model.baseline_ready() ? 1 : 0, g_state->last_level_valid ? static_cast<double>(g_state->last_level) : -999.0,
                (g_state->model.baseline_ready() ? static_cast<double>(g_state->model.rf_baseline_dbfs()) : -999.0), static_cast<double>(rf_rise_threshold_db(g_state->model.sensitivity())),
                static_cast<unsigned long>(g_hooks.frequency_hz ? g_hooks.frequency_hz() : 0),
                g_hooks.demod && g_hooks.receiver_ready && g_hooks.receiver_ready() ? g_hooks.demod() : "--",
                g_state->sweep.running() ? 1 : 0, static_cast<unsigned long>(g_state->sweep.hops()),
                static_cast<unsigned long>(g_state->sweep.stalls()), static_cast<unsigned long>(g_state->sweep.unobserved_ms()),
                static_cast<unsigned long>(g_touches), static_cast<unsigned>(g_last_action), static_cast<unsigned long>(g_state->sweep.resume_hz()));
}

}  // namespace orcsdr::dead_air_app
