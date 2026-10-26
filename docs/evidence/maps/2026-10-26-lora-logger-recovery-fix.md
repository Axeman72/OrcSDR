# LoRa logger automatic recovery repair - October 26, 2026

The old task removed queued records even after a logging failure had latched its
error flag. The focused production-task fault test reproduces this loss before
the source repair. This repairs that behavior; it does not establish the cause
of the original SD error or restore the nine historical missing records.

## Source change

- The worker retries failed storage operations after two seconds, yielding while
  waiting. It does not restart a FreeRTOS task for each received packet.
- One current record is held in PSRAM until both journal and CSV writes sync.
  The existing 32-record queue remains intact behind it. Only actual queue
  overflow increments dropped records; a write failure retains the current row.
- A pending append remembers both file boundaries. A retry reopens the same
  journal/CSV in update mode and rewinds only the current record's uncommitted
  suffix. Successful earlier rows are preserved, and partial writes or failed
  syncs do not create duplicate rows on retry.
- Unexpected file sizes prevent truncation rather than deleting unowned data.
  Permanent storage failure remains visible while the queue is held.
- Each completed record now syncs both files rather than batching a five-second
  sync. The cost of this durability change needs reception testing on COM17.
- OFF suspends error retries; ON signals the worker to retry immediately, after
  closing failed streams. The UI does not clear the error under a live writer.
- `LORA_SD_LOG STATUS` adds pending, retry_attempts, last_error_stage and
  last_errno. `LORA_SD_LOG_ERROR`, `LORA_SD_LOG_RETRY` and
  `LORA_SD_LOG_RECOVERED` identify the failing operation and automatic recovery.
- Driver, Wi-Fi/C6, DSP, map rendering and map downloads are outside this change.

## Verification

`bash -lc 'python3 tests/lora_log_recovery_test.py'` exercises the production task
and real serializers, with fault injection through the storage API. All 19 cases
pass with AddressSanitizer/UndefinedBehaviorSanitizer:

short journal/CSV writes, a partial CSV header, journal/CSV sync failures, CSV and
journal open failures, permanent failure, logging OFF, unknown extra suffix data,
queue overflow, timer wraparound, failed rollback, transient stat error, OFF/ON
recovery, and failures on later records preserving previously committed rows.
The first test fails on the old task because it discards the current and queued
records rather than recovering. The original red log is retained.

Existing storage update-mode and nine FT8 log-flush fault checks also pass.
Cadence/history/lifecycle and LoRa handoff/startup checks are run separately.

Raw logs beside invocation in
`F:/AI/OrcSDR-TEMP/maps-integration-completion`:

- `lora-log-recovery-red-actual-20261026.log`
- `lora-log-recovery-19-cases-20261026.log`
- `lora-log-recovery-focused-host-20261026.log`
- `lora-log-recovery-cadence-host-20261026.log`
- `lora-log-recovery-final-host-20261026.log`

Native linking, PSRAM placement and device validation are pending at this source
checkpoint. No physical card-failure or power-cut durability pass is claimed.
Pending RAM data is not nonvolatile storage, and sustained unavailability can
fill the bounded queue. The earlier watchdog incident remains open separately.
