# ESP32-P4 map performance evidence

Status: baseline only. No optimization result is claimed.

## Measured source/device baseline

| Metric | COM17 | COM39 |
|---|---:|---:|
| Uptime ms | 143725176 | 14898322 |
| Free heap bytes | 17677000 | 2502180 |
| Internal free bytes | 44663 | 37455 |
| Largest internal block | 31744 | 31744 |
| Internal DMA free | 10699 | 3587 |
| Largest DMA block | 10240 | 1728 |
| PSRAM free bytes | 17642988 | 2468300 |
| Largest PSRAM block | 17301504 | 2359296 |
| Active mode | Airband | LoRa |

Different firmware/runtime/modes mean these are not comparable performance samples. No device was reset or paused to normalize them.

Existing retained main-code build artifact from the preceding local session is 4,617,600 bytes, versus 6,291,456-byte factory capacity (1,673,856 bytes partition headroom; 1,411,712 bytes below the release limit after its 256 KiB reserve). That is artifact size evidence, not proof of live flash hash. Fresh ESP-IDF 5.5.4 baseline build passed with the same 4,617,600-byte application size. Rollback binary/ELF are retained at `.local/maps/baseline.bin` and `.local/maps/baseline.elf`; full command log is `maps-baseline-build.log`.

SD throughput, map frame/render phases, PMTiles lookup/inflate/MVT times, display flush, SD seek/read amplification, loaded-RF map FPS, allocation-failure behavior and physical visual acceptance: UNTESTED in this session. No throughput benchmark is run on COM17's historical SD card without permission.

OrcMaps host baseline: CMake Debug build in `.local/baseline-host`; ctest 1/1 target passed. This does not establish ESP32-P4 timings. Existing OrcMaps docs contain historical static-render measurements; those must not be presented as this integration's measurements.

## Experiment protocol

Use deterministic PMTiles golden fixtures and fixed viewports. Record archive/source/license, target adapter, viewport and cold/warm state. Instrument lookup, inflate/decode, feature render and flush separately, plus internal DMA free/largest, PSRAM, app size, RF/audio/IQ drops, USB overruns and watchdogs. Test missing/corrupt pack, manifest/hash mismatch, allocation failure, antimeridian and zoom bounds.

Prototype one measured bottleneck at a time. Promote only a reproducible gain without RF regressions. Do not add speculative cache/dirty rectangles/pools before baseline evidence. Read-only host fixtures first; flash, pack provisioning and SD writes need separate user approval once artifacts are reviewable.

## Host Springfield render baseline

The released OrcMaps pack-inspect pipeline rendered the tracked 3,507,636-byte Springfield PMTiles at 44.0500/-123.0220, zoom 14, orcsdr-dark to `.local/springfield-baseline.ppm`. Reported host times in ms: enumerate 4.674; lookup 106.396; decompress 8.038; decode 15.362; translate 6.643; classify 0.780; render 5.532; image write 10719.703. Concurrent native build and Windows/WSL file I/O make this unsuitable for a speed comparison. No optimization is promoted from it. Log: OrcMaps worktree `maps-pack-inspect-baseline.log`.

Fresh baseline application SHA256: `0f3f95656394080133ce1f74ed8408cb8a1031d9cb3414f021a9ee7e40b9470b`. This is a host-built rollback artifact, not a newly flashed device image.

## First integrated worker build (2026-10-17)

Source: shared map worker, region/legacy adapter, and LoRa caller; OrcMaps pinned
to `70fea367909c74794bb69c7aed97e0f11f0a9387` (v0.2.1 miniz fix plus reviewed PR4).
`& ./apps/orcsdr-tab5/tools/build-tab5-idf.ps1` passed after correcting two
compiler errors. Log: `maps-worker-native-recheck.log` in the invocation directory.
Application: 4,741,456 bytes (`0x485950`), factory headroom 1,550,000 bytes;
release guard headroom 1,287,856 bytes. SHA-256:
`bf6d2a57e6ab85852c063f1750c00abab5313a0702ad81248f3e36bfd430f9a1`.
Linked-image symbol gate: PASS, two bundled miniz symbols, none ROM-absolute.
No device flashed; active-RF render timing, memory, visual correctness, cancellation
and allocation-failure recovery remain UNTESTED on COM17/COM39.
The worker uses one private PSRAM sprite, a depth-one latest-request queue and
per-feature cancellation/yield hooks. UI sprite transfer stays on the UI owner.
No map performance optimization has been claimed from these build results.

## Allocation-safe candidate build

Approved full native rebuild: `& ./apps/orcsdr-tab5/tools/build-tab5-idf.ps1 -WorldMap F:/AI/OrcSDR-TEMP/maps-integration-completion/.local/maps/world/world-overview-z4.pmtiles`.
Final corrected run log: `maps-allocation-final-build.log` (invocation directory),
reviewed copy `docs/evidence/maps/2026-10-17-allocation-final-build.log`.

- Application: 5,020,576 bytes; factory headroom 1,270,880 bytes; release guard
  headroom 1,008,736 bytes. Fresh-baseline application growth: 402,976 bytes.
- SHA-256: `395c18dea8cad6d7a1454a9ac88fd6e2d5508b5e8373d1eebf8af499121a678f`.
- C++ exceptions enabled; emergency allocation reserve configured to 1,024 bytes.
  This is a runtime/build change, not a measured speed improvement. All actual
  device heap/DMA/PSRAM impacts remain UNTESTED with this image.
- Requested/locked/fetched OrcMaps agreement: PASS at c3cc09de3882149e03c7c00d7897c6882b324c6b.
- Linked-image miniz check: PASS; two bundled symbols, none ROM-absolute.
- All other locked dependencies, including RTL-SDR, Hosted, wifi_remote and mdns,
  compare equal to the pre-refresh lock. ESP-IDF stays 5.5.4.
- `cmd /d /c .local\maps\run-pipeline.cmd`: 3/3 host targets pass with ASAN and
  exception unwinding, including injected allocation failure followed by recovery.
  Log `maps-allocation-recovery-host.log`; reviewed copy under docs/evidence/maps.

The first exception-enabled build completed but retained the old engine lock despite
its new manifest. It is not the candidate for the cleanup fix. A new configure gate
rejects this mismatch; the recorded gate fails against the stale lock and passes
against the corrected lock/fetched component. Resolution also tried to advance two
unrelated floating dependencies; that build was stopped and their exact prior lock
records restored. The final build above contains only the intended OrcMaps update.

The new frame/inventory adapters catch bad_alloc, free the failed frame and allow a
new request/rescan. Device directory/text-file handles close through exception-safe
ownership. Host failure injection is not a substitute for ESP32 allocation-failure
acceptance. No device reset, flash, SD change, radio pause, or profile run occurred.

## Bounded hardware acceptance plan (awaiting target approval)

1. Confirm COM39's physical identity. USB serial observed E8:F6:0A:E2:D6:03;
   COM17 is 30:ED:A0:E2:E6:95. Port naming alone is insufficient authorization.
2. Stop reception for an approved backup/flash window. Save an opaque full-flash
   recovery image and its hash locally; do not decode or print private NVS values.
3. Provision only the reviewed bootloader, table, app and world archive at their
   stated offsets. Do not erase the chip, write NVS, modify SD, or update C6.
4. Capture boot/version/self-check, stored location and map inventory before and
   after. Existing legacy setup must bypass first-run and preserve saved location.
5. Exercise Settings picker Cancel / explicit Confirm, wizard Back / Skip / Cancel /
   Quick Start / Finish, optional Network return, keyboard and touch. Reboot after
   progress checkpoint to test recovery only with explicit device-test authorization.
6. On active LoRa (current COM39 reception), pan/zoom and alternate Home, maps and
   dashboard for a bounded session; collect RTL_MAP_STATUS, RTL_HEALTH, RTL_MEM STATUS,
   RTL_STATS and UI status before/after. Record map total/lookup/stream/render/I/O/push
   timing, cached pushes, PSRAM and largest DMA block alongside drops/USB/reset data.
7. Exercise missing/corrupt pack, no-SD and allocation failure only in an approved
   controlled setup. Do not alter COM17's historical packs or remove either card
   while workers use it. FM audio continuity needs its own short receiving sample;
   no new overnight soak is required by this plan.

`stream_us` includes decoder work, lookup/read work inside streaming, and worker yields;
it is not a disjoint CPU category. Cached pushes count reuse of the single completed
frame, not a rendered-tile LRU hit rate. `bytes/reads/io_us` describe archive reads,
not measured SD bus throughput. Exact exclusive decode/SD throughput, p50/p95 frame
intervals, active-RF continuity, physical screen correctness, and low-memory recovery
are UNTESTED until target logs establish them. No optimization has been promoted.

## Final location-safe candidate

The focused safety review found a separate interrupted-location-save issue. After
its single-record persistence fix, four host targets pass (`maps-location-after.log`),
including the interruption regression that failed beforehand. Incremental native
commands: from the exported ESP-IDF environment,
`cmake --build apps/orcsdr-tab5/build-native-hosted3 -- -j4`.
Both `maps-location-native-build.log` and `maps-candidate-final-build.log` completed
successfully; the last run includes the new record in legacy-device classification.
The linked miniz gate passes. The pin gate and unchanged-dependency checks also pass.

Final app: 5021344 bytes; factory headroom 1270112 bytes;
release guard headroom 1007968 bytes. SHA-256:
`bd4000ae7c29d9608eddc7e03d2288ba65c101bb964c2274509fbd3248680fe1`.
`docs/evidence/maps/2026-10-17-candidate-manifest.json` records this final candidate;
the allocation-only build above is historical intermediate evidence. No flash,
physical interrupted-save test or active-RF profile has occurred.

## COM39 acceptance attempt and recovery — 2026-10-17

The user confirmed COM39 / USB serial `E8:F6:0A:E2:D6:03` and approved candidate
`4971ee5b`, world provisioning, a reception pause, recovery backup and bounded
UI/radio checks. COM17 (`30:ED:A0:E2:E6:95`) was not opened or flashed in this attempt.
No C6 update, SD overwrite, NVS erase or whole-chip erase was performed.

Pre-flash commands used an existing authenticated key without `PAIR`; `RTL_STOP`
completed with `RTL_STOP_RESULT ESP_OK`. `SD_LIST` found the existing FM settings,
their backup and `/orcsdr/lora_packets_v2.csv` (42177 bytes at inventory time).
`RTL_LOCATION STATUS` reports the temporary lookup result, **not canonical saved
location**; the saved coordinate was not obtained before the failed boot. Do not
claim a physical location-migration test passed.

Recovery command (Python is the IDF 5.5 environment):
`python -m esptool --chip esp32p4 --port COM39 --baud 921600 --after no_reset read_flash 0x0 0x1000000 .local/maps/com39-recovery/20261017-1726/full-flash.bin`.
Result: 16777216 bytes read successfully in 139.1 seconds. SHA-256:
`90b19ec8fe53ff1f807ba8f68e9872cb36601de6aa07a42bc356ad86c06e3751`.
Opaque recovery/NVS/core-dump files remain ignored locally; no private NVS was
decoded. Partition comparison showed every existing partition unchanged and only
the new world partition added at `0x650000`.

The explicit COM39 `write_flash` command used the four offsets listed above.
Bootloader, table, application and world each reported `Hash of data verified`.
Before application boot, `verify_flash` of the opaque saved NVS at `0x9000` and
saved core dump at `0x610000` both reported `verify OK (digest matched)`.

**Hardware acceptance FAILED.** The candidate rebooted before `app_main` with:
`main_task: Could not reserve internal/DMA pool (error 0x101)` followed by `abort()`.
ESP-IDF was reserving the unchanged 40 KiB internal/DMA pool. No map worker, setup
flow or active-RF map profile ran. The first monitor command also hit an independent
host `esp-idf-monitor` 1.9.0 Logger error with `--disable-address-decoding`; retrying
without that flag captured the device startup failure. These are separate failures.

The candidate startup dump was saved separately, then original bootloader,
partition table, factory region and prior core dump were restored from the backup.
All four restoration writes passed hash verification; the unchanged NVS again
passed `verify_flash` before boot. The unused world bytes remain in flash, but the
restored partition table does not expose them as an installed map.

Recovery checks: `UI_SELF_CHECK` returned all six fields equal to 1. Subsequent
`RTL_DRIVER STATUS` reported STREAMING, 906875000 Hz, effective_sps=959967,
bytes=290455552, overruns=0 and drops=0. `RTL_LORA_STATS` reported events=2,
crc_ok=2, messages=2, failures=0 and drops=0. This is real LoRa reception on the
restored firmware, not map candidate acceptance. The previously connected LoRa
antenna was not physically changed during the test.

At restored uptime 160903 ms: internal_free=36487, internal_largest=31744,
dma_free=2627, dma_largest=2432, psram_free=2127968,
psram_largest=2097152. This snapshot is not an overnight soak or memory-leak test.

Logs were saved in the worktree root, the invocation directory:
`maps-com39-preflash-20261017.log`, `maps-com39-rom-identity-20261017.log`,
`maps-com39-backup-20261017.log`, `maps-com39-recovery-check-20261017.log`,
`maps-com39-flash-20261017.log`, `maps-com39-preservation-20261017.log`,
`maps-com39-boot-20261017.log`, `maps-com39-boot-retry-20261017.log`,
`maps-com39-startup-dump-20261017.log`, `maps-com39-restore-20261017.log`,
`maps-com39-recovery-nvs-20261017.log`, `maps-com39-restored-health-20261017.log`
and `maps-com39-restored-final-20261017.log`. Reviewed copies/extracts are under
`docs/evidence/maps/2026-10-17-com39-*`.

### Prepared placement adjustment — not flashed

`ui/map_service.cpp` places the 340-byte canvas object and 256-byte archive-path
buffer in PSRAM. They are UI/worker state, not DMA descriptors or ISR state.
Both objects' linked addresses move from internal `0x4ff...` to PSRAM `0x485...`;
`.dram1.bss` shrinks from 171300 to 170708 bytes, recovering 592 bytes after
alignment. The DMA reserve, exception reserve, radio code and other locked
dependencies are unchanged. The startup failure itself is confirmed; whether this
extra margin is sufficient is **UNTESTED on hardware**.

Exact incremental test: from the exported IDF environment,
`cmake --build apps/orcsdr-tab5/build-native-hosted3 -- -j4`.
Result: PASS, 10 build steps; linked miniz gate PASS; application remains 5021344
bytes. `python .local/maps/check_startup_placement.py` passed symbol-placement and
positive internal-BSS-recovery assertions. Logs:
`maps-startup-placement-build-20261017.log` and
`maps-startup-placement-check-20261017.json`. The initial placement assertion assumed
all 596 payload bytes would shrink BSS; section alignment gives 592, so the check
was corrected to verify actual placement and positive recovery.

Prepared application SHA-256:
`91ed8cef4026447684b7f7f8ce81eb8c8a1bef7e2feb486d5e732acfc573a02d`.
Historical `4971ee5b` binary/ELF remain `.local/maps/candidate.*`; the new artifacts
are `.local/maps/startup-placement.*`. No repeated full rebuild or full regression
suite was run. Next gate: approve the new candidate's COM39 flash, verify boot
first, then resume the bounded setup/location/map/radio acceptance sequence.
