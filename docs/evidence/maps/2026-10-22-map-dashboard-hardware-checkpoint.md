# COM17 map/dashboard checkpoint — October 22, 2026

Source candidate: `050442b0`, including ADS-B range correction `19856eea`.
Current source also contains HTTP-adapter tests `15f1f0bf`; these tests do not
change the installed firmware. Device: COM17, USB serial `30:ED:A0:E2:E6:95`.
COM39 was not used. This record supersedes the earlier pending-port status.

## Installed artifact and preservation

Artifact directory: `.local/maps/lora-map-controls-050442b0-20261021/`.
App size: 5,139,024 bytes; factory headroom: 1,152,432 bytes.
App SHA-256: `43f431100b0e44c80b4e9b108f1543956e27e7ef5a04a74e7c8ae52428776565`.
ELF SHA-256: `ab18583d9848be536da9c1cf23d17210aee641800c0a424d3caa58e9ff224a32`.
The app-only flash and saved non-app preservation checks pass. NVS, embedded
world, C6, bootloader and partition table were preserved; SD packs were not
replaced. The 40 KiB internal DMA reserve is unchanged. No occupied-port process
was killed; the port became available before the update.

Invocation from the integration worktree root:

```text
python .local/maps/flash_lora_map_controls_20261021.py
python .local/maps/com17_label_bench.py lora-map-controls-device-plan-20261021.json lora-map-controls-device-20261021.log
python .local/maps/com17_label_bench.py adsb-map-range-device-plan-20261021.json adsb-map-range-device-20261021.log
python .local/maps/com17_label_bench.py lora050442b0-retained-sd-plan-20261022.json lora050442b0-retained-sd-device-20261022.log
```

Flash logs: root `lora-map-controls-verify-previous-20261021.log`,
`lora-map-controls-app-write-20261021.log` and
`lora-map-controls-preservation-20261021.log`, plus separate backup logs.
Native startup capture is in
`apps/orcsdr-tab5/lora-map-controls-boot-native-monitor-20261021.log`.
Log names retain their original invocation dates.

## Bounded results

- LoRa plan exits 0: pan, zoom in/out, receiver recenter and saved-coordinate
  preservation pass. Follow-on rejects a selected node without a usable
  position; follow-off passes. This does not test positioned-node following.
  Final streaming rate is 954,869 samples/s with zero observed driver drops or
  overruns. Free PSRAM is 4,128,356 bytes; largest DMA block is 8,192 bytes.
- ADS-B plan exits 0: dedicated range command, existing Settings action and
  routed +/− touches agree; malformed/out-of-range values are rejected.
  Original 100 NM radius is restored, with persistence confirmed after recovery
  reboot. This does not establish aircraft-overlay or RF decoding acceptance.
- Cached preview has only two samples: same-scale paint p50/p95 is
  25,999/26,785 microseconds; scaled paint is 28,793/67,197 microseconds.
  Dwell-inclusive update intervals are not FPS. Physical touch latency was not
  measured. The representative 33 ms gate remains open.
- SD substeps pass: uniquely named absent-checked 65,536-byte scratch file
  `/orcsdr/maplabels_330606cb_lora050442b0_20261022.bin` writes and reads back
  exactly. SHA-256 is
  `f8a38b49873588eea0fa55c178b2ad20a05eb7f3eccf55623323405fdec79fe9`.
  Map memory 751,376 bytes and LoRa IQ memory 15,840,000 bytes remain retained.
  Free PSRAM before/after is 4,079,552 bytes; largest DMA block is 8,192 bytes.
  Reception was stopped for transfer. The scratch file remains on SD.

## FT8 transition failure and recovery

The SD plan **exits 1** after the passing SD substeps. Opening FT8 starts its
runtime and live drawing; during subsequent serial authentication/snapshot
output, the device reports 416,052 bytes free and panics with a stack-protection
fault. This follows a long elapsed interval, not a controlled overnight soak.

Reported task: `main`; SP `0x4ff4c7e0`, bounds
`0x4ff4bfb0`–`0x4ff4c7e0`; MEPC `0x4ff002c0`. Address resolution identifies the
SDK interrupt handler. The native main task is configured for 16 KiB; the
reported interrupt-context bounds are not proof of ordinary main-task overflow.
Neither low-memory causation nor the faulting application call is established.

Read-only dump extraction (verified device identity first):

```text
python -m esptool --chip esp32p4 --port COM17 --baud 921600 --before default_reset --after no_reset read_flash 0x610000 0x40000 .local/maps/lora-ft8-stack-crash-20261022/coredump.bin
```

Saved partition is 262,144 bytes, with a 63,296-byte dump. SHA-256:
`dbba16e20e8640c335ce8fc4af24a4082f26a1a8c2ef867542fb3726f7889bc2`.
Evidence: `lora-ft8-coredump-read-20261022.log`,
`lora-ft8-coredump-convert-20261022.log`,
`lora-ft8-coredump-decoded-20261022.log`,
`lora-ft8-coredump-decoded-gdb16-20261022.log` and
`lora-ft8-coredump-gdb-backtraces-20261022.log` in the worktree root.
SDK decoders fail with `KeyError: current-thread-id`; manual GDB identifies
interrupt context but cannot unwind the faulting application chain. Raw dump
artifacts remain local and are not committed.

After controlled recovery, this command exits 0:

```text
python .local/maps/com17_label_bench.py lora-ft8-recovery-plan-20261022.json lora-ft8-recovery-device-20261022.log
```

Startup capture:
`apps/orcsdr-tab5/lora-ft8-recovery-boot-native-monitor-20261022.log`.
FT8 is restored at 7.070 MHz, streaming about 234,478 samples/s with zero observed
driver drops/overruns. Free PSRAM is 18,472,120 bytes, largest block 18,350,080;
largest DMA block 8,192; main stack high-water mark 1,896. Saved receiver
coordinates and ADS-B radius remain unchanged. Exact previous gain/AGC values
were not restored: final AUTO gain is 0 and RTL AGC is off. These are last logged
observations, not a claim about subsequent live device state.

## HTTP adapter host gate

`15f1f0bf` adds tests of production `ui/map_http_stream.cpp` against fake SDK
calls: TLS configuration, resume headers, redirects, bounded reads, rejection
and cleanup. Normal and ASan/UBSan executions both exit 0 with
`MAP_HTTP_STREAM_PASS`; no network is used.

```text
cmake -S apps/orcsdr-tab5/tests/maps -B .local/maps/labels-host
cmake --build .local/maps/labels-host --target orcsdr_map_http_stream_tests -j2
.local/maps/labels-host/orcsdr_map_http_stream_tests
cmake -S apps/orcsdr-tab5/tests/maps -B .local/maps/map-serial-asan-20261021
cmake --build .local/maps/map-serial-asan-20261021 --target orcsdr_map_http_stream_tests -j2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 .local/maps/map-serial-asan-20261021/orcsdr_map_http_stream_tests
```

Commands ran through WSL from the worktree root. Root logs are
`map-http-stream-{host,sanitizer}-{configure,build,test}-20261021.log`.
Real HTTPS transfer and device installation/recovery remain unaccepted.

## Next gate

Investigate the FT8 transition using the preserved dump and allocation evidence;
do not assume a stack-size change fixes it. Full global-pack device quality,
positioned overlays/follow, setup recovery, actual HTTPS delivery and sustained
RF/performance acceptance remain open. No main merge or publication occurred.
