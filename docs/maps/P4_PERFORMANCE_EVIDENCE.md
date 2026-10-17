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
