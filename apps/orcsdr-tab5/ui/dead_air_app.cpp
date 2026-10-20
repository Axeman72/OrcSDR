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

#include <cstdarg>
#include <mbedtls/sha256.h>

#include "dead_air_audio.hpp"
#include "dead_air_clip.hpp"
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
constexpr uint32_t kMaxClipsPerSession = 40;  // about 40 MB; keeps a busy session from filling the card
constexpr uint64_t kRingMarginSamples = 8192;  // never trust PCM this close to being overwritten by the next DSP block
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

// Level buckets from the DSP thread (single producer) to the UI task (single consumer).
struct AudioRing {
  static constexpr uint32_t kSize = 128;
  AudioBucket slots[kSize];
  std::atomic<uint32_t> head{0};
  AudioMeter meter;  // touched by the DSP thread only
  // Rolling copy of the demodulated PCM for event clips (PSRAM). The DSP thread is the only writer.
  int16_t* pcm = nullptr;
  uint32_t capacity = 0;                 // samples
  std::atomic<uint64_t> written{0};      // total samples ever written
  std::atomic<uint64_t> run_start{0};    // first sample of the current uninterrupted run of audio
  std::atomic<bool> capture{false};
  uint32_t last_write_ms = 0;            // DSP thread only
};

// Hand-off between the UI task (plans clips) and the SD worker (writes them).
struct ClipTicket {
  ClipJob job;
  bool abandon = false;                  // the planner gave up: record why, write no file
  const char* reason = nullptr;          // static string
  char demod[8]{};
  uint32_t frequency_hz = 0;
  bool speaker_on = false;
};
struct ClipResult {
  bool ok = false;
  bool abandoned = false;
  bool truncated = false;
  char name[32]{};
  char sha[65]{};
  uint32_t samples = 0;
};
struct ClipMail {
  std::atomic<uint8_t> state{0};         // 0 empty, 1 ticket ready, 2 worker busy, 3 result ready
  ClipTicket ticket;
  ClipResult result;
  int16_t chunk[4096];
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
  bool rx_settled = false;  // receiver ready and past its settling window (refreshed every tick)
  uint32_t opened_ms = 0;
  uint32_t last_observe_ms = 0, last_snapshot_ms = 0, settle_until_ms = 0;
  uint32_t last_signature = 0, config_revision = 0, last_counter = 0;
  bool have_signature = false;
  float last_level = 0.0f;
  bool last_level_valid = false;
  uint32_t last_total = 0;
  AudioRing audio;
  ClipPlanner planner;
  ClipTicket pending_meta;  // receiver details captured when the current clip began
  ClipMail mail;
  bool clips_enabled = true;
  std::atomic<uint32_t> clips_saved{0}, clips_failed{0};
  uint32_t clips_refused_no_audio = 0, clips_refused_busy = 0, clips_refused_limit = 0;
  ClipInfo recent[kClipInfoCount];
  uint8_t recent_count = 0;
  AudioDetector audio_detector;
  uint32_t audio_tail = 0;
  float audio_env[kBins]{};
  size_t audio_env_count = 0;
  float audio_rms = kAudioSilenceDbfs, audio_peak = kAudioSilenceDbfs;
  uint32_t last_audio_ms = 0;
  bool audio_flowing = false;
  Page last_page = Page::overview;
  Snapshot pushed;  // what the dashboard was last given; refresh decisions compare against this
  uint32_t last_level_refresh_ms = 0;
  Sweep sweep;
  uint8_t sweep_preset = 0;
  uint16_t sweep_dwell_ms = 500;
};

Hooks g_hooks;
State* g_state = nullptr;
bool g_open = false;
std::atomic<AudioRing*> g_ring{nullptr};
std::atomic<bool> g_audio_wanted{false};
uint8_t g_last_action = 0;     // diagnostic: last touch action kind seen by the app
uint32_t g_touches = 0;

bool ready_hooks() {
  return g_hooks.receiver_ready && g_hooks.frequency_hz && g_hooks.demod && g_hooks.sample_rate &&
         g_hooks.config_signature && g_hooks.rf_sample_counter && g_hooks.rf_level_dbfs;
}

void set_state(Worker& w, RecorderState s) { w.state.store(static_cast<uint8_t>(s), std::memory_order_release); }


// ---- Clip writing (SD worker only) -------------------------------------------------------------------------------
void json_append(char* buf, size_t cap, size_t& len, const char* fmt, ...) __attribute__((format(printf, 4, 5)));
void json_append(char* buf, size_t cap, size_t& len, const char* fmt, ...) {
  if (len >= cap) return;
  va_list args;
  va_start(args, fmt);
  const int n = std::vsnprintf(buf + len, cap - len, fmt, args);
  va_end(args);
  if (n > 0) len = std::min(cap - 1, len + static_cast<size_t>(n));
}

void json_seqs(char* buf, size_t cap, size_t& len, const ClipJob& job) {
  json_append(buf, cap, len, "[");
  for (uint8_t i = 0; i < job.seq_count; ++i) json_append(buf, cap, len, "%s%lu", i ? "," : "", static_cast<unsigned long>(job.trigger_seqs[i]));
  json_append(buf, cap, len, "]");
}

bool journal_line(State& s, const char* line, size_t len) {
  return len > 0 && s.sink.append(line, len) && s.sink.sync();
}

// Writes one clip as a WAV file plus one journal record. Every outcome is recorded: a clip that could not be written
// leaves a clip_failed record naming the reason, never a missing or half-valid file.
void write_clip(State& s) {
  ClipMail& m = s.mail;
  const ClipTicket& t = m.ticket;
  ClipResult& r = m.result;
  r = ClipResult{};
  char line[768];
  size_t len = 0;
  const auto fail = [&](const char* reason) {
    r.ok = false;
    r.abandoned = true;
    len = 0;
    json_append(line, sizeof(line), len, "{\"v\":1,\"type\":\"clip_failed\",\"event_seqs\":");
    json_seqs(line, sizeof(line), len, t.job);
    json_append(line, sizeof(line), len, ",\"trigger_count\":%lu,\"reason\":\"%s\"}\n", static_cast<unsigned long>(t.job.trigger_count), reason);
    (void)journal_line(s, line, len);
    s.clips_failed.fetch_add(1, std::memory_order_relaxed);
  };
  if (t.abandon) { fail(t.reason ? t.reason : "abandoned"); return; }
  AudioRing& ring = s.audio;
  const ClipJob& job = t.job;
  const uint32_t samples = static_cast<uint32_t>(job.end - job.start);
  storage::FileSystem& fs = storage::filesystem();
  if (!ring.pcm || !(fs.exists("/orcsdr/dead_air/clips") || fs.mkdir("/orcsdr/dead_air/clips"))) { fail("no_clip_folder"); return; }
  std::snprintf(r.name, sizeof(r.name), "s%010lu_e%lu.wav", static_cast<unsigned long>(s.sink.session_id()),
                static_cast<unsigned long>(job.trigger_seqs[0]));
  char path[96];
  std::snprintf(path, sizeof(path), "/orcsdr/dead_air/clips/%s", r.name);
  storage::File file = fs.open(path, FILE_WRITE, true);
  if (!file) { fail("file_open_failed"); return; }
  mbedtls_sha256_context sha;
  mbedtls_sha256_init(&sha);
  bool ok = mbedtls_sha256_starts(&sha, 0) == 0;
  uint8_t header[kWavHeaderBytes];
  build_wav_header(header, samples, kClipSampleRate);
  ok = ok && file.write(header, sizeof(header)) == sizeof(header) && mbedtls_sha256_update(&sha, header, sizeof(header)) == 0;
  const char* reason = "write_failed";
  uint64_t pos = job.start;
  const uint32_t cap = ring.capacity;
  while (ok && pos < job.end) {
    const uint32_t n = static_cast<uint32_t>(std::min<uint64_t>(sizeof(m.chunk) / sizeof(m.chunk[0]), job.end - pos));
    // The ring keeps overwriting its oldest samples; a sample is still intact only while it is less than one ring behind.
    if (ring.written.load(std::memory_order_acquire) - pos > cap - kRingMarginSamples) { ok = false; reason = "ring_overrun"; break; }
    const uint32_t from = static_cast<uint32_t>(pos % cap);
    const uint32_t first = std::min<uint32_t>(n, cap - from);
    std::memcpy(m.chunk, ring.pcm + from, first * sizeof(int16_t));
    if (n > first) std::memcpy(m.chunk + first, ring.pcm, (n - first) * sizeof(int16_t));
    if (ring.written.load(std::memory_order_acquire) - pos > cap - kRingMarginSamples) { ok = false; reason = "ring_overrun"; break; }
    const size_t bytes = static_cast<size_t>(n) * sizeof(int16_t);
    ok = file.write(reinterpret_cast<const uint8_t*>(m.chunk), bytes) == bytes &&
         mbedtls_sha256_update(&sha, reinterpret_cast<const uint8_t*>(m.chunk), bytes) == 0;
    pos += n;
  }
  uint8_t digest[32]{};
  ok = ok && mbedtls_sha256_finish(&sha, digest) == 0;
  mbedtls_sha256_free(&sha);
  const size_t expected = kWavHeaderBytes + static_cast<size_t>(samples) * sizeof(int16_t);
  ok = ok && file.size() == expected && file.sync();
  file.close();
  if (!ok) {
    (void)fs.remove(path);  // never leave a partial clip that looks like evidence
    fail(reason);
    return;
  }
  for (int i = 0; i < 32; ++i) std::snprintf(r.sha + i * 2, 3, "%02x", digest[i]);
  r.ok = true;
  r.truncated = job.truncated;
  r.samples = samples;
  len = 0;
  json_append(line, sizeof(line), len, "{\"v\":1,\"type\":\"clip\",\"event_seqs\":");
  json_seqs(line, sizeof(line), len, job);
  json_append(line, sizeof(line), len,
              ",\"trigger_count\":%lu,\"file\":\"clips/%s\",\"bytes\":%lu,\"sha256\":\"%s\",\"format\":\"pcm_s16le\","
              "\"rate_hz\":%lu,\"channels\":1,\"samples\":%lu,\"duration_ms\":%lu,\"pre_ms\":%lu,\"post_ms\":%lu,"
              "\"truncated\":%s,\"source\":\"receiver_demod\",\"demod\":\"%s\",\"frequency_hz\":%lu,\"speaker\":\"%s\","
              "\"processing\":\"none by Dead Air; receiver demodulator output\",\"trigger_mono_ms\":%lu,\"unix_time\":",
              static_cast<unsigned long>(job.trigger_count), r.name, static_cast<unsigned long>(expected), r.sha,
              static_cast<unsigned long>(kClipSampleRate), static_cast<unsigned long>(samples),
              static_cast<unsigned long>(static_cast<uint64_t>(samples) * 1000u / kClipSampleRate),
              static_cast<unsigned long>(kClipPreMs), static_cast<unsigned long>(kClipPostMs), job.truncated ? "true" : "false",
              t.demod, static_cast<unsigned long>(t.frequency_hz), t.speaker_on ? "on" : "muted",
              static_cast<unsigned long>(job.trigger_ms));
  if (job.unix_time) json_append(line, sizeof(line), len, "%lu}\n", static_cast<unsigned long>(job.unix_time));
  else json_append(line, sizeof(line), len, "null}\n");
  if (!journal_line(s, line, len)) {
    r.ok = false;  // the file exists on the card but the log could not say so: report it as failed rather than half-done
    s.clips_failed.fetch_add(1, std::memory_order_relaxed);
    return;
  }
  s.clips_saved.fetch_add(1, std::memory_order_relaxed);
}

void write_clip_summary(State& s) {
  char line[200];
  const int n = std::snprintf(line, sizeof(line),
                              "{\"v\":1,\"type\":\"clip_summary\",\"enabled\":%s,\"saved\":%lu,\"failed\":%lu,"
                              "\"refused_no_audio\":%lu,\"refused_busy\":%lu,\"refused_limit\":%lu}\n",
                              s.clips_enabled ? "true" : "false", static_cast<unsigned long>(s.clips_saved.load()),
                              static_cast<unsigned long>(s.clips_failed.load()),
                              static_cast<unsigned long>(s.clips_refused_no_audio), static_cast<unsigned long>(s.clips_refused_busy),
                              static_cast<unsigned long>(s.clips_refused_limit));
  if (n > 0) (void)journal_line(s, line, static_cast<size_t>(n));
}

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
    if (s->mail.state.load(std::memory_order_acquire) == 1) {
      s->mail.state.store(2, std::memory_order_relaxed);
      write_clip(*s);
      s->mail.state.store(3, std::memory_order_release);
    }
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
    if (s->mail.state.load(std::memory_order_acquire) == 1) {  // a ticket posted just before the stop
      s->mail.state.store(2, std::memory_order_relaxed);
      write_clip(*s);
      s->mail.state.store(3, std::memory_order_release);
    }
    write_clip_summary(*s);
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


// ---- Clip planning (UI task) -------------------------------------------------------------------------------------
void post_ticket(State& s, const ClipTicket& t) {
  s.mail.ticket = t;
  s.mail.state.store(1, std::memory_order_release);
}

void fill_meta(ClipTicket& t) {
  std::snprintf(t.demod, sizeof(t.demod), "%s", g_hooks.demod ? g_hooks.demod() : "--");
  t.frequency_hz = g_hooks.frequency_hz ? g_hooks.frequency_hz() : 0;
  t.speaker_on = g_hooks.sound_enabled ? g_hooks.sound_enabled() : false;
}

// Called for every event as it is logged. Only measured detections and investigator marks start a clip.
void trigger_clip(State& s, const Event& e) {
  if (!s.clips_enabled || !s.audio.pcm) return;
  if (e.kind != Kind::rf_rise && e.kind != Kind::audio_burst && e.kind != Kind::manual_marker) return;
  const bool was_idle = s.planner.state() == ClipState::idle;
  if (was_idle && s.clips_saved.load(std::memory_order_relaxed) + s.clips_failed.load(std::memory_order_relaxed) >= kMaxClipsPerSession) {
    ++s.clips_refused_limit;  // the event is still logged; only its clip is skipped
    return;
  }
  switch (s.planner.trigger(s.audio.written.load(std::memory_order_acquire), s.audio.run_start.load(std::memory_order_relaxed),
                            s.audio_flowing, e.sequence, millis(), e.unix_time)) {
    case ClipReject::none:
      if (was_idle) fill_meta(s.pending_meta);  // what the receiver was doing when the clip began
      break;
    case ClipReject::no_audio: ++s.clips_refused_no_audio; break;
    case ClipReject::busy: ++s.clips_refused_busy; break;
  }
}

void on_event(void* context, const Event& e) {
  State* s = static_cast<State*>(context);
  enqueue_event(&s->queue, e);  // the journal first: this never depends on audio
  trigger_clip(*s, e);
}

void remember_clip(State& s, const ClipResult& r) {
  for (size_t i = kClipInfoCount - 1; i > 0; --i) s.recent[i] = s.recent[i - 1];
  ClipInfo& c = s.recent[0];
  c = ClipInfo{};
  std::snprintf(c.name, sizeof(c.name), "%s", r.name);
  std::snprintf(c.sha8, sizeof(c.sha8), "%.8s", r.sha);
  c.duration_ds = static_cast<uint16_t>(std::min<uint64_t>(65535, static_cast<uint64_t>(r.samples) * 10u / kClipSampleRate));
  c.truncated = r.truncated;
  s.recent_count = static_cast<uint8_t>(std::min<size_t>(kClipInfoCount, s.recent_count + 1u));
}

// Collects a finished clip from the SD worker, then asks the planner whether another is ready to hand over.
void service_clips(State& s, uint32_t now) {
  if (s.mail.state.load(std::memory_order_acquire) == 3) {
    const ClipResult r = s.mail.result;
    if (r.ok) remember_clip(s, r);
    s.mail.state.store(0, std::memory_order_relaxed);
    if (s.planner.state() == ClipState::writing) s.planner.finished();
  }
  if (s.mail.state.load(std::memory_order_acquire) != 0) return;
  ClipJob job;
  ClipFail failure = ClipFail::none;
  if (s.planner.poll(s.audio.written.load(std::memory_order_acquire), s.audio.run_start.load(std::memory_order_relaxed),
                     s.audio_flowing, now, &job, &failure)) {
    ClipTicket t{};
    t.job = job;
    std::memcpy(t.demod, s.pending_meta.demod, sizeof(t.demod));
    t.frequency_hz = s.pending_meta.frequency_hz;
    t.speaker_on = s.pending_meta.speaker_on;
    post_ticket(s, t);
  } else if (failure != ClipFail::none) {
    ClipTicket t{};
    t.job = job;
    t.abandon = true;
    t.reason = "audio_stopped";
    post_ticket(s, t);
  }
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
  if (xTaskCreatePinnedToCoreWithCaps(worker_main, "dead_air_sd", 12288, g_state, 1, &w.task, 0,
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
  // Audio is only available while demodulated audio is actually flowing (speaker on, or the Audio tab open).
  n.audio_available = s.audio_flowing;
  n.audio_level_dbfs = {};
  n.audio_envelope_valid = s.audio_flowing && s.audio_env_count >= 2;
  if (s.audio_flowing) {
    n.audio_level_dbfs.present = true;
    n.audio_level_dbfs.valid = true;
    n.audio_level_dbfs.value = s.audio_rms;
    n.audio_level_dbfs.baseline_valid = s.audio_detector.baseline_ready();
    n.audio_level_dbfs.baseline = s.audio_detector.baseline();
  }
  n.audio_peak_dbfs = s.audio_peak;
  n.clips_enabled = s.clips_enabled && s.audio.pcm != nullptr;
  n.clip_state = static_cast<uint8_t>(s.planner.state());
  n.clips_saved = s.clips_saved.load(std::memory_order_relaxed);
  n.clips_failed = s.clips_failed.load(std::memory_order_relaxed);
  n.clips_refused = s.clips_refused_no_audio + s.clips_refused_busy + s.clips_refused_limit;
  for (size_t i = 0; i < kClipInfoCount; ++i) n.clips[i] = s.recent[i];
  n.clip_count = s.recent_count;
  n.audio_baseline_ready = s.audio_detector.baseline_ready();
  n.audio_baseline_dbfs = s.audio_detector.baseline();
  std::memcpy(n.audio_envelope, s.audio_env, sizeof(n.audio_envelope));
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

// Structural changes that need the page recomposed.
bool visibly_changed(const Snapshot& a, const Snapshot& b) {
  return a.receiver_ready != b.receiver_ready || a.receiver_settled != b.receiver_settled ||
         a.frequency_hz != b.frequency_hz || a.sound_enabled != b.sound_enabled ||
         a.battery_percent != b.battery_percent ||
         (a.session_elapsed_s != b.session_elapsed_s && dead_air::page() == Page::overview) ||
         a.recorder_state != b.recorder_state || a.recorder_saved != b.recorder_saved ||
         a.recorder_missing != b.recorder_missing || a.recorder_dropped != b.recorder_dropped ||
         a.recorder_session != b.recorder_session || a.rf_level_dbfs.valid != b.rf_level_dbfs.valid ||
         a.rf_level_dbfs.baseline_valid != b.rf_level_dbfs.baseline_valid ||
         a.audio_available != b.audio_available || a.clips_enabled != b.clips_enabled || a.clip_state != b.clip_state ||
         a.clips_saved != b.clips_saved || a.clips_failed != b.clips_failed || a.clips_refused != b.clips_refused ||
         a.sweeping != b.sweeping || a.sweep_hops != b.sweep_hops || a.sweep_preset != b.sweep_preset ||
         a.sweep_dwell_ms != b.sweep_dwell_ms ||
         a.spectrum_valid != b.spectrum_valid || std::strcmp(a.demod, b.demod) != 0;
}

// Numeric readings shown only on the Overview; they change constantly, so they refresh at most twice a second.
bool levels_moved(const Snapshot& a, const Snapshot& b) {
  if (dead_air::page() != Page::overview) return false;
  const auto q = [](float v) { return static_cast<int>(std::lround(v * 0.5f)); };  // 2 dB steps
  return q(a.rf_level_dbfs.value) != q(b.rf_level_dbfs.value) || q(a.audio_level_dbfs.value) != q(b.audio_level_dbfs.value);
}

}  // namespace

void install(const Hooks& hooks) { g_hooks = hooks; }
bool active() { return g_open && dead_air::active(); }

void open() {
  if (!ready_hooks() || !ensure_state()) return;
  State& s = *g_state;
  const uint32_t now = millis();
  s.model.reset();
  s.model.set_listener(on_event, &s);
  s.opened_ms = now;
  s.last_observe_ms = s.last_snapshot_ms = 0;
  s.settle_until_ms = now + kSettleMs;
  s.have_signature = false;
  s.last_level_valid = false;
  s.spectrum_have = false;
  s.last_total = 0;
  s.snapshot = Snapshot();
  s.audio.meter.configure(48000);
  if (!s.audio.pcm) {  // the clip ring: 12 s of demodulated PCM in PSRAM, allocated once
    const uint32_t cap = kClipSampleRate / 1000u * kClipRingMs;
    s.audio.pcm = static_cast<int16_t*>(heap_caps_malloc(static_cast<size_t>(cap) * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    s.audio.capacity = s.audio.pcm ? cap : 0;
  }
  s.audio.written.store(0, std::memory_order_relaxed);
  s.audio.run_start.store(0, std::memory_order_relaxed);
  s.audio.last_write_ms = 0;
  s.audio.capture.store(s.clips_enabled && s.audio.pcm != nullptr, std::memory_order_relaxed);
  s.planner.configure(kClipSampleRate, kClipPreMs, kClipPostMs);
  s.mail.state.store(0, std::memory_order_relaxed);
  s.clips_saved.store(0, std::memory_order_relaxed);
  s.clips_failed.store(0, std::memory_order_relaxed);
  s.clips_refused_no_audio = s.clips_refused_busy = s.clips_refused_limit = 0;
  s.recent_count = 0;
  s.audio.head.store(0, std::memory_order_relaxed);
  s.audio_tail = 0;
  s.audio_detector.reset();
  s.audio_env_count = 0;
  s.audio_flowing = false;
  s.last_audio_ms = 0;
  s.last_level_refresh_ms = 0;
  (void)start_worker();
  if (g_hooks.set_demod) g_hooks.set_demod(0);  // NFM, the browse band's own demodulator
  build_snapshot(now);
  s.snapshot.revision = 1;
  s.pushed = s.snapshot;
  screens::begin_transition(screens::Id::dead_air, now);
  dead_air::attach_events(&s.model);
  dead_air::enter(s.snapshot);
  screens::finish_transition();
  g_ring.store(&s.audio, std::memory_order_release);  // the DSP thread may now offer audio
  g_open = true;
}

void close() {
  if (!g_open || !g_state) return;
  g_open = false;
  g_ring.store(nullptr, std::memory_order_release);
  g_state->audio.capture.store(false, std::memory_order_relaxed);
  if (g_state->planner.state() == ClipState::capturing && g_state->mail.state.load(std::memory_order_acquire) == 0) {
    ClipTicket t{};
    t.job = g_state->planner.pending();
    t.abandon = true;
    t.reason = "session_closed";
    post_ticket(*g_state, t);  // the worker records why this clip never happened
  }
  g_state->planner.cancel();
  g_audio_wanted.store(false, std::memory_order_relaxed);
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
  s.rx_settled = ready && static_cast<int32_t>(now - s.settle_until_ms) >= 0;
  if (!s.have_signature || signature != s.last_signature || !ready) {
    // A retune, gain/rate change or receiver (re)start invalidates the baseline and the settling window.
    if (!s.have_signature || signature != s.last_signature || s.last_level_valid) {
      ++s.config_revision;
      if (s.audio.pcm) s.audio.run_start.store(s.audio.written.load(std::memory_order_relaxed), std::memory_order_relaxed);
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
  // Audio level buckets from the DSP thread: update the envelope and run the activity detector.
  bool audio_changed = false;
  float fresh_bars[8];
  size_t fresh_count = 0;
  const bool on_audio_tab = screens::owns(screens::Id::dead_air) && dead_air::page() == Page::audio;
  g_audio_wanted.store(on_audio_tab || (s.clips_enabled && s.audio.pcm && ready), std::memory_order_relaxed);
  if (dead_air::page() != s.last_page) {
    s.last_page = dead_air::page();
    if (on_audio_tab) audio_changed = true;  // paint the live part of the tab right after it opens
  }
  {
    const uint32_t head = s.audio.head.load(std::memory_order_acquire);
    if (head - s.audio_tail > AudioRing::kSize) {  // the UI fell behind: drop the oldest and restart the detector
      s.audio_tail = head - AudioRing::kSize;
      s.audio_detector.reset();
    }
    while (s.audio_tail != head) {
      const AudioBucket b = s.audio.slots[s.audio_tail % AudioRing::kSize];
      ++s.audio_tail;
      s.audio_rms = b.rms_dbfs;
      s.audio_peak = b.peak_dbfs;
      s.last_audio_ms = now;
      const float norm = std::clamp((b.peak_dbfs + 70.0f) / 70.0f, 0.0f, 1.0f);
      std::memmove(s.audio_env, s.audio_env + 1, (kBins - 1) * sizeof(float));
      s.audio_env[kBins - 1] = norm;
      s.audio_env_count = std::min<size_t>(s.audio_env_count + 1, kBins);
      if (fresh_count == 8) { std::memmove(fresh_bars, fresh_bars + 1, 7 * sizeof(float)); --fresh_count; }
      fresh_bars[fresh_count++] = norm;
      audio_changed = true;
      AudioFinding f;
      // Not while a sweep is hopping: every hop is a different channel.
      if (s.rx_settled && !s.sweep.running() &&
          s.audio_detector.observe(b, now, s.config_revision, s.model.sensitivity(), &f)) {
        Event e{};
        e.monotonic_ms = now;
        e.unix_time = g_hooks.utc_epoch ? g_hooks.utc_epoch() : 0;
        e.frequency_hz = ready ? g_hooks.frequency_hz() : 0;
        e.source = Source::audio;
        e.kind = Kind::audio_burst;
        e.value = f.rms_dbfs;
        e.reference = f.baseline_dbfs;
        e.threshold = f.threshold_db;
        (void)s.model.append_measured(e);
      } else if (!s.rx_settled || s.sweep.running()) {
        s.audio_detector.reset();
      }
    }
    const bool flowing = s.last_audio_ms != 0 && now - s.last_audio_ms < 500;
    if (flowing != s.audio_flowing) {
      s.audio_flowing = flowing;
      audio_changed = true;
      if (!flowing) s.audio_detector.reset();
    }
  }
  service_clips(s, now);
  if (on_audio_tab && audio_changed) {
    AudioView v{};
    v.envelope = s.audio_env;
    v.count = s.audio_env_count;
    v.fresh = fresh_bars;
    v.fresh_count = fresh_count;
    v.flowing = s.audio_flowing;
    v.rms_dbfs = s.audio_rms;
    v.peak_dbfs = s.audio_peak;
    v.baseline_ready = s.audio_detector.baseline_ready();
    v.baseline_dbfs = s.audio_detector.baseline();
    (void)dead_air::draw_audio_frame(v, now);
  }
  if (!screens::owns(screens::Id::dead_air)) return;
  if (now - s.last_snapshot_ms < 100) return;
  const uint32_t revision = s.snapshot.revision;
  build_snapshot(now);
  const bool events_changed = s.model.total() != s.last_total;
  const bool slow_due = levels_moved(s.snapshot, s.pushed) && now - s.last_level_refresh_ms >= 500;
  const bool changed = visibly_changed(s.snapshot, s.pushed) || events_changed || slow_due;
  if (changed) {
    s.last_total = s.model.total();
    s.snapshot.revision = revision + 1;
    s.pushed = s.snapshot;
    s.last_snapshot_ms = now;
    if (slow_due) s.last_level_refresh_ms = now;
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
    case ActionKind::toggle_clips:
      g_state->clips_enabled = !g_state->clips_enabled;
      g_state->audio.capture.store(g_state->clips_enabled && g_state->audio.pcm != nullptr, std::memory_order_relaxed);
      break;
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

void offer_audio(const int16_t* pcm, size_t count) {
  AudioRing* r = g_ring.load(std::memory_order_acquire);
  if (!r || !pcm || count == 0) return;
  if (r->pcm && r->capacity && r->capture.load(std::memory_order_relaxed)) {
    const uint32_t now = millis();
    uint64_t w = r->written.load(std::memory_order_relaxed);
    if (w == 0 || now - r->last_write_ms > 300) r->run_start.store(w, std::memory_order_relaxed);  // a new uninterrupted run
    r->last_write_ms = now;
    size_t done = 0;
    while (done < count) {
      const uint32_t from = static_cast<uint32_t>(w % r->capacity);
      const size_t n = std::min<size_t>(count - done, r->capacity - from);
      std::memcpy(r->pcm + from, pcm + done, n * sizeof(int16_t));
      done += n;
      w += n;
    }
    r->written.store(w, std::memory_order_release);
  }
  AudioBucket out[4];
  const size_t m = r->meter.feed(pcm, count, out, 4);
  for (size_t i = 0; i < m; ++i) {
    const uint32_t head = r->head.load(std::memory_order_relaxed);
    r->slots[head % AudioRing::kSize] = out[i];
    r->head.store(head + 1, std::memory_order_release);
  }
}

bool wants_audio() { return g_audio_wanted.load(std::memory_order_relaxed); }

bool wants_spectrum() {
  return active() && screens::owns(screens::Id::dead_air) && dead_air::page() == Page::spectrum;
}

void feed_spectrum(const float* levels, size_t bins, uint32_t sample_rate_sps) {
  (void)sample_rate_sps;
  if (!g_open || !g_state || !levels || bins < kBins) return;
  const float offset_db = 20.0f * std::log10(kFullScaleAmplitude * (static_cast<float>(bins) - 1.0f) * 0.5f);
  State& s = *g_state;
  // Max-pool the receiver's FFT to the trace resolution, then derive the coarse bins from those points.
  float points[kTracePoints];
  for (size_t i = 0; i < kTracePoints; ++i) {
    const size_t a = i * bins / kTracePoints, b = std::max(a + 1, (i + 1) * bins / kTracePoints);
    float peak = -200.0f;
    for (size_t k = a; k < b && k < bins; ++k) peak = std::max(peak, levels[k]);
    points[i] = peak - offset_db;  // per-bin power, relative dBFS
  }
  for (size_t i = 0; i < kBins; ++i) {
    const size_t a = i * kTracePoints / kBins, b = std::max(a + 1, (i + 1) * kTracePoints / kBins);
    float peak = -200.0f;
    for (size_t k = a; k < b && k < kTracePoints; ++k) peak = std::max(peak, points[k]);
    s.spectrum[i] = peak;
  }
  s.spectrum_ms = millis();
  s.spectrum_have = true;
  ++s.spectrum_seq;
  // Only a settled receiver is drawn, and never under the volume tray.
  if (s.rx_settled && !(g_hooks.overlay_open && g_hooks.overlay_open()))
    (void)dead_air::draw_spectrum_frame(points, kTracePoints, s.config_revision, s.spectrum_ms);
}

uint32_t flood_manual_marks(uint32_t count) {
  if (!g_open || !g_state) return 0;
  count = std::min<uint32_t>(count, 500);
  const uint32_t now = millis();
  const uint32_t unix_time = g_hooks.utc_epoch ? g_hooks.utc_epoch() : 0;
  const uint32_t hz = g_hooks.frequency_hz ? g_hooks.frequency_hz() : 0;
  for (uint32_t i = 0; i < count; ++i) g_state->model.mark_manual(now, unix_time, hz);
  return count;
}

void describe(char* out, size_t size) {
  if (!out || !size) return;
  if (!g_state) { std::snprintf(out, size, "DEAD_AIR open=0"); return; }
  const Worker& w = g_state->worker;
  const PaintStats ps = dead_air::paint_stats();
  std::snprintf(out, size,
                "DEAD_AIR open=%d recorder=%u session=%lu saved=%lu missing=%lu dropped=%lu events=%lu baseline=%d level=%.1f base_dbfs=%.1f thr_db=%.0f hz=%lu mode=%s sweep=%d hops=%lu stalls=%lu unobs_ms=%lu touches=%lu last_action=%u resume=%lu spec_seq=%lu spec_valid=%d paint_us=%lu/%lu/%lu max_push_us=%lu px=%lu rects=%lu whole=%lu part=%lu frame_us=%lu max_frame_us=%lu frames=%lu audio=%d rms=%.1f peak=%.1f abase=%d clip=%u cs=%lu cf=%lu cr=%lu",
                g_open ? 1 : 0, static_cast<unsigned>(w.state.load()), static_cast<unsigned long>(w.session.load()),
                static_cast<unsigned long>(w.saved.load()), static_cast<unsigned long>(w.missing.load()),
                static_cast<unsigned long>(w.dropped.load()), static_cast<unsigned long>(g_state->model.total()),
                g_state->model.baseline_ready() ? 1 : 0, g_state->last_level_valid ? static_cast<double>(g_state->last_level) : -999.0,
                (g_state->model.baseline_ready() ? static_cast<double>(g_state->model.rf_baseline_dbfs()) : -999.0), static_cast<double>(rf_rise_threshold_db(g_state->model.sensitivity())),
                static_cast<unsigned long>(g_hooks.frequency_hz ? g_hooks.frequency_hz() : 0),
                g_hooks.demod && g_hooks.receiver_ready && g_hooks.receiver_ready() ? g_hooks.demod() : "--",
                g_state->sweep.running() ? 1 : 0, static_cast<unsigned long>(g_state->sweep.hops()),
                static_cast<unsigned long>(g_state->sweep.stalls()), static_cast<unsigned long>(g_state->sweep.unobserved_ms()),
                static_cast<unsigned long>(g_touches), static_cast<unsigned>(g_last_action), static_cast<unsigned long>(g_state->sweep.resume_hz()),
                static_cast<unsigned long>(g_state->spectrum_seq), g_state->snapshot.spectrum_valid ? 1 : 0,
                static_cast<unsigned long>(ps.compose_us), static_cast<unsigned long>(ps.diff_us),
                static_cast<unsigned long>(ps.push_us), static_cast<unsigned long>(ps.push_us_max),
                static_cast<unsigned long>(ps.pixels), static_cast<unsigned long>(ps.rects),
                static_cast<unsigned long>(ps.whole_pushes), static_cast<unsigned long>(ps.partial_pushes),
                static_cast<unsigned long>(ps.frame_us), static_cast<unsigned long>(ps.frame_us_max),
                static_cast<unsigned long>(ps.frames),
                g_state->audio_flowing ? 1 : 0, static_cast<double>(g_state->audio_rms), static_cast<double>(g_state->audio_peak),
                g_state->audio_detector.baseline_ready() ? 1 : 0,
                static_cast<unsigned>(g_state->planner.state()), static_cast<unsigned long>(g_state->clips_saved.load()),
                static_cast<unsigned long>(g_state->clips_failed.load()),
                static_cast<unsigned long>(g_state->clips_refused_no_audio + g_state->clips_refused_busy));
}

}  // namespace orcsdr::dead_air_app
