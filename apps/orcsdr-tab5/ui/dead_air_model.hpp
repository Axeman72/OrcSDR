#pragma once
#include <cstddef>
#include <cstdint>

namespace orcsdr::dead_air {

// This model contains observations, not paranormal classifications. No mock
// readings are ever used by the live dashboard.
enum class Page : uint8_t { overview, spectrum, audio, environment, events, review, count };
enum class Sensitivity : uint8_t { quiet, standard, sensitive };
enum class Source : uint8_t { rf, audio, environment, manual };
enum class Kind : uint8_t { rf_rise, audio_burst, temperature_change, humidity_change,
                            pressure_change, manual_marker };
enum class Status : uint8_t { unavailable, warming_up, current, stale };

struct Reading {
  float value = 0.0f;
  float baseline = 0.0f;
  uint32_t age_ms = 0;
  bool present = false;
  bool valid = false;
  bool baseline_valid = false;
};

struct Event {
  uint32_t sequence = 0;
  uint32_t monotonic_ms = 0;
  uint32_t unix_time = 0;  // 0 means wall clock not verified
  uint32_t frequency_hz = 0;
  Source source = Source::rf;
  Kind kind = Kind::rf_rise;
  float value = 0.0f;
  float reference = 0.0f;
  float threshold = 0.0f;
  Sensitivity sensitivity = Sensitivity::standard;
};

constexpr size_t kBins = 96;
constexpr size_t kEventCapacity = 48;
struct Snapshot {
  bool receiver_ready = false;
  bool recording = false;
  bool scanning = false;
  bool audio_available = false;
  bool sound_enabled = true;
  uint32_t frequency_hz = 0;
  uint32_t sample_rate_sps = 0;
  uint32_t session_elapsed_s = 0;
  uint32_t unix_time = 0;
  int battery_percent = -1;
  char demod[8] = "--";
  Reading rf_level_dbfs{};    // relative dBFS, NEVER dBm
  Reading audio_level_dbfs{}; // relative dBFS
  Reading temperature_c{};
  Reading humidity_rh{};
  Reading pressure_hpa{};
  Reading motion_g{};        // device movement, not room movement
  // Spectrum and waveform bins are accepted only when a real source supplies
  // them. No fabricated waterfall, FFT, or voice scores.
  float spectrum_dbfs[kBins]{};
  float audio_envelope[kBins]{};
  bool spectrum_valid = false;
  bool audio_envelope_valid = false;
  bool environment_sensor_attached = false;
  uint32_t revision = 0;
};

struct RfObservation {
  uint32_t monotonic_ms = 0;
  uint32_t unix_time = 0;
  uint32_t frequency_hz = 0;
  uint32_t configuration_revision = 0;
  float level_dbfs = 0.0f;
  bool valid = false;
  bool settled = false;
};

class EventModel {
 public:
  void reset();
  void set_sensitivity(Sensitivity sensitivity);
  Sensitivity sensitivity() const { return sensitivity_; }
  void observe_rf(const RfObservation& sample);
  // Used by a UI-task callback after an independently verified detector fired.
  // Never use this for interpreted labels or unsupported environmental sensors.
  bool append_measured(const Event& event);
  void mark_manual(uint32_t monotonic_ms, uint32_t unix_time, uint32_t tuned_hz);
  size_t count() const { return count_; }
  uint32_t total() const { return total_; }
  const Event* newest(size_t index) const;
  bool baseline_ready() const { return baseline_count_ >= kWarmupCount; }
  float rf_baseline_dbfs() const { return baseline_; }
  bool self_check() const;
 private:
  void push(Event event);
  void reset_detector(uint32_t revision, uint32_t frequency);
  static constexpr uint8_t kWarmupCount = 12;
  Event events_[kEventCapacity]{};
  size_t count_ = 0, next_ = 0;
  uint32_t total_ = 0;
  uint32_t config_revision_ = 0, tuned_hz_ = 0;
  uint32_t candidate_since_ = 0, cooldown_until_ = 0;
  uint32_t last_valid_ms_ = 0;
  bool have_valid_sample_ = false;
  uint8_t baseline_count_ = 0;
  float baseline_ = 0.0f;
  bool have_tune_ = false, pending_ = false, latched_ = false;
  Sensitivity sensitivity_ = Sensitivity::standard;
};

// A source may be physically present but still warming up or stale. In every
// case except current, the dashboard shows "--" and no event can be generated.
Status reading_status(const Reading& reading, uint32_t stale_after_ms = 10000);
float rf_rise_threshold_db(Sensitivity sensitivity);
const char* source_name(Source source);
const char* kind_name(Kind kind);

}  // namespace orcsdr::dead_air
