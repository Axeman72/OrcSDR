#include "ntp_sync.hpp"

#include "time_service.hpp"

#include <esp_sntp.h>
#include <esp_timer.h>

#include <ctime>
#include <sys/time.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace orcsdr::ntp_sync {
namespace {

uint32_t now_ms() { return static_cast<uint32_t>(esp_timer_get_time() / 1000); }

constexpr uint32_t kTimeoutMs = 20000;
constexpr const char* kServer = "pool.ntp.org";

State g_state = State::idle;
uint32_t g_started_ms = 0;
bool g_result_pending = false;
bool g_result_ok = false;
uint32_t g_last_success_utc = 0;
bool g_attempted = false;

void finish(bool ok) {
  esp_sntp_stop();
  if (!ok) time_service::resync_system_from_rtc();   // SNTP already moved the system clock; put it back in step with the RTC
  if (ok) g_last_success_utc = static_cast<uint32_t>(time(nullptr));
  g_state = ok ? State::done : State::failed;
  g_result_ok = ok;
  g_result_pending = true;
}

}  // namespace

bool start(bool network_ready) {
  if (g_state == State::syncing || !network_ready) return false;
  g_result_pending = false;   // a result from an earlier attempt must not be reported for this one
  if (esp_sntp_enabled()) esp_sntp_stop();
  esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
  esp_sntp_setservername(0, kServer);
  sntp_set_sync_status(SNTP_SYNC_STATUS_RESET);
  esp_sntp_init();
  g_started_ms = now_ms();
  g_attempted = true;
  g_state = State::syncing;
  return true;
}

uint32_t last_success_utc() { return g_last_success_utc; }

void forget_last_success() {
  if (g_state == State::syncing) {   // an attempt in flight would finish later, rewrite the clock and mark it synced again
    esp_sntp_stop();
    time_service::resync_system_from_rtc();
    g_state = State::idle;
  }
  g_last_success_utc = 0;
  g_attempted = false;
}

uint32_t ms_since_last_attempt() { return g_attempted ? now_ms() - g_started_ms : 0xFFFFFFFFu; }

bool last_attempt_failed() { return g_attempted && g_state == State::failed; }

void poll() {
  if (g_state != State::syncing) return;
  if (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
    // SNTP has set the system clock to the millisecond, but the hardware RTC only counts whole seconds and restarts its
    // sub-second counter when it is written. Writing it mid-second would leave the RTC (and every later boot) off by the
    // fraction of a second, so wait for the next second boundary and write that second then (at most about 1 s once).
    timeval tv{};
    time_service::wait_for_second_boundary(&tv);
    const time_t now = tv.tv_sec;
    finish(now > 0 && time_service::set_utc(static_cast<uint32_t>(now)));
  } else if (now_ms() - g_started_ms > kTimeoutMs) {
    finish(false);
  }
}

State state() { return g_state; }

bool take_result(bool* ok) {
  if (!g_result_pending) return false;
  g_result_pending = false;
  if (ok != nullptr) *ok = g_result_ok;
  return true;
}

}  // namespace orcsdr::ntp_sync
