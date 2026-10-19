# SD card reliability and regression checks

This is the shared record for Tab5 SD changes. Map availability, successful
stdio writes, filesystem durability, adequate DMA memory and RF continuity are
separate checks. A passing screenshot download does not qualify the whole card.

## Failures already investigated

| Area | Finding and correction | Evidence to preserve |
|---|---|---|
| Shared high-volume writes | Unaligned PSRAM fell into single-sector copies; writable streams now use optional 32 KiB cache-aligned PSRAM buffers. Removing buffering is not a fix. | [Write-performance investigation](TAB5_SD_PERFORMANCE.md); `f1588da5` |
| FT8 log | OrcDial bridge consumed scarce internal DMA memory. Queue placement recovered headroom; file append semantics were corrected. FT8 stages CSV in PSRAM and waits for its DMA threshold. | [2026-10-08 FT8 entry](ft8/CHANGELOG.md#2026-10-08--decode-log-to-the-sd-card-works-internal-dma-memory-recovered); `431b1f46` |
| LoRa persistence | An open buffered log needs fflush plus fsync to commit data/metadata. Shared File::sync does both; worker syncs journal and CSV at five-second intervals and when logging stops. Close/sync errors remain observable. | `eaa51423`, `d7deb59a`; [history behavior](lora-history.md), [device checks](lora-history-validation-2026-10-16.md) |
| SSTV archive | Archive worker uses its own cache-aligned stdio buffers, retained until fclose. Existing tests exercise missing/full/flaky/slow storage, partial reads/writes, retry and temporary-file recovery. | `ui/sstv_archive.cpp`; `tests/sstv_archive_test.cpp` under the Tab5 app |
| Map-retained SD read | Historical cache-sync invalid-address error preceded SD error 0x102. SDK's alignment-only check can admit non-DMA CPU heaps. Candidate adds a reachability/range check before using direct DMA. | [Source investigation](evidence/maps/2026-10-17-sd-retained-source-result.md); [5c6f2853 build/device checks](evidence/maps/2026-10-17-sd-guard-device-result.md) passed within their bounds |

These are different failure mechanisms. Reducing map memory or slowing SD is not
a universal correction. The old recorded pointer is unavailable, so the precise
historical map-read allocation remains unproven even though the unsafe-buffer
path is reproduced in a host test.

## Requirements for future SD changes

- Keep Tab5 card on Slot 0, C6 on Slot 1, existing pins/LDO4 and no-format mount.
  Do not reinitialize the Hosted controller while mounting the card.
- Check DMA reachability and cache alignment, including the complete byte range.
  CPU-accessible TCM/RTC is not equivalent to DMA-capable memory. Keep valid
  aligned PSRAM on the direct path; use the SDK copy path for incompatible buffers.
- Preserve aligned stdio buffering and its lifetime through fclose in both the
  shared storage wrapper and SSTV archive. Allocation failure keeps default libc
  buffering; do not add another permanent internal DMA pool without measurements.
- Preserve append mode for logs. A new record must not truncate earlier records.
  Short writes, sync/close errors, full/missing cards and incomplete records must
  remain visible. A write counter alone does not establish durable storage.
- Keep SD work off decoder/audio callbacks. Preserve FT8 staging, LoRa's logging
  queue and SSTV's archive worker. Record free and largest internal DMA blocks,
  PSRAM, queue drops and RF counters under actual load.
- Keep CRC-valid reception, archive contents and reconstructed NODEINFO/position
  state distinct. Do not alter old formats or erase packs to make a test pass.
- Test files must use an explicitly reserved scratch path that is verified absent
  before creation. Existing SD self-check/benchmark commands use fixed names and
  delete them; do not run them on valuable cards without checking those names.

## 2026-10-19 map-download staging source checkpoint

The new transfer path uses the shared `File` wrapper and its existing buffering,
sync and DMA guard; it does not bypass those protections with direct SDMMC or
raw stdio writes. Its caller-owned transfer buffer must be PSRAM on P4. The
40 KiB internal reserve and existing FT8/LoRa/SSTV write paths are unchanged.

One additive operation, `File::truncate`, is shrink-only and is used to restore
an owned staging file to its last synced resume checkpoint. It flushes, refuses
extension, invokes SDK FAT/VFS `ftruncate` and restores the cursor. It does not
truncate logs, existing map packs or user-selected files. No card contents were
changed while preparing/testing this source.

Two alternating checked resume records prevent a torn newer record from being
the sole recovery source. Bad responses are rejected before changing a staging
tail. Completion requires full SD read-back hashing plus an actual PMTiles
header/root check. Prepared files remain in a hidden subdirectory outside flat
map discovery; version replacement still needs a recovery transaction.

The focused host transfer harness and shared DMA test each pass 1/1; three P4
syntax checks pass. The host storage adapter flushes stdio and injects faults;
it does **not** execute real FAT fsync/truncate or establish power-loss durability.
Physical-card behavior, retained-map pressure, largest internal DMA block,
radio/logging continuity and controlled interruption remain separate gates.
See [transfer evidence and exact logs](evidence/maps/2026-10-19-map-transfer-staging.md).

## 2026-10-17 COM39 device checkpoint

`5c6f2853` builds and boots; retained-map read/write and real LoRa CSV readback
pass. Before flashing, old 99affb1b already showed logging error=true and 33
lost log records despite a healthy RTL stream. The new cycle ends logging
ready=true/error=false/dropped=0; 411 CSV rows include two new RF records and
preserve all 409 earlier rows. This does not recover records previously dropped.
The historical rejected pointer is unrecorded; do not treat this as proof of
all past failures or all-mode SD reliability. [Commands, actual counters,
limitations and logs](evidence/maps/2026-10-17-sd-guard-device-result.md).

## Repeatable host gate

From the repository root on Linux/WSL:

```bash
bash tools/test-sd-storage.sh
```

This builds/runs five focused test programs with AddressSanitizer and UBSan:
DMA/range/alignment, benchmark write-size plan, LoRa archive/cache/CSV behavior,
SSTV archive fault handling, and the actual FT8 log flush function extracted
from main.cpp with injected write/sync/close failures. Build/run logs and a summary stay in the
invocation directory; names carry an explicit UTC timestamp. It does not access
COM ports or physical SD cards. Temporary host binaries are retained in the
directory printed by the runner. A failure stops the gate and identifies its log.

The SD storage workflow runs this gate on relevant pull requests and main pushes
and saves logs even after failures. The workflow is prepared locally; no GitHub
run or required branch-protection check is claimed until published/configured.
The buffer test is also registered as `sd_dma_buffer` in the maps CTest project.

Actual 2026-10-17 local result: **4/4 PASS**, including the SSTV slow-card test.
[Host summary](evidence/maps/2026-10-17-sd-storage-host.log).
The first GCC compile found a missing initializer_list include in the new test;
that portability issue was corrected before this successful run.

These tests do not execute ESP-IDF SDMMC, hardware allocation, or the LoRa
worker's real fsync. The FT8 harness executes its production flush function
against fault-injected file operations, not a physical card.

On 2026-10-18 the FT8 fault test reproduced an existing false-success path:
a failed close still advanced the written count and discarded staged rows.
FT8 now checks the CSV header, data write, fsync and close before clearing its
stage; failures retain rows and report their actual operation. All five host
programs pass. Hardware validation of this additional change is pending.
A partially committed append can be duplicated on retry; this change does not
make CSV appends transactional or qualify sudden power loss/card removal.

## Device acceptance before declaring reliability

October 19 map installation follow-up: hidden staging now has journaled
activation/recovery source. Focused host tests cover interrupted first install
and replacement, corrupted journals/bytes, status errors and external metadata
conflicts. Recovery runs before PMTiles discovery and fails visibly rather than
treating SD I/O errors as absent files. Previous archives and unmanaged maps are
retained. These are host/P4-syntax results, not physical SD power-loss proof;
installer hardware validation and worker/UI wiring remain open.
[Exact commands, logs and limitations](evidence/maps/2026-10-19-map-activation-recovery.md).

| Check | Current candidate status |
|---|---|
| Normal native link/boot and unchanged reserve/firmware identity | PASS on COM39, 5c6f2853, full 40 KiB reserve |
| Full map retained, repeated checksum-verified SD reads | PASS: three 2.76 MB reads, frame 1,417,728 bytes retained, no reclaim |
| Bounded scratch-file writes/readback with retained map | PASS: unique absent-checked 131,109-byte file, exact readback |
| Internal DMA/PSRAM, navigation and LoRa continuity during maps | Bounded checks PASS; DMA margins remain tight; one abort after intentional reception stop is recorded |
| SSTV archive save/reload with map memory pressure | Host faults pass; candidate device behavior untested |
| FT8 append/flush/readback and staged-row handling under pressure | Historical validation exists; candidate device behavior untested |
| LoRa append/sync, stop/readback, reboot replay with NODEINFO preserved | Two new RF rows read back; old 409-row prefix intact; 56 archived nodes replayed. Reboot of the two new records not repeated |
| Card full/removal and power-cut durability | Separate destructive/fault testing needed; not authorized by a normal flash |

For each device cycle save target/firmware hash, card/bus details, commands,
timings, data hashes, memory/counters, actual failures and restoration steps.
Preserve recovery/NVS/SD data; do not fold an overnight soak or card fault
campaign into a bounded test approval. No guarantee against all future SD failures
is inferred from a host gate or one device sample.
