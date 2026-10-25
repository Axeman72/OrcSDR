# LoRa workspace shortage and candidate correction

## Device observations

COM17, USB serial `30:ED:A0:E2:E6:95`, app source b0542203, driver 0.9.3,
Blog V4 and indoor 915 MHz whip, US LongFast slot 20 / 906.875 MHz:

- Initial failure: PSRAM free 1,257,860 bytes, largest block 327,680;
  native failure counter 10, no CRC-valid packets. Sample delivery continued at
  approximately 960 ksps with zero driver drops/overruns.
- Controlled restart: a CRC-valid telemetry packet decoded, `Voltage 4.15V`.
  A second CRC-valid packet contained a position. This was recovery, not a fix.
- Opening/cancelling the full-screen picker retained 3,884,032 bytes of map state.
  Free PSRAM fell to 586,112 bytes / largest block 237,568; six subsequent
  decoder attempts failed before preamble decoding.
- Authenticated `RTL_MEM RECLAIM` released exactly 3,884,032 map bytes. At
  14:39:04 a subsequent capture decoded one packet with CRC valid, without
  another restart. The user subsequently confirmed additional traffic.

The observed allocation pressure prevents preprocessing. It does not establish
that every missed transmission has this cause. In particular, no exact-content
controlled TX acceptance has passed for the release matrix.

COM39, serial `E8:F6:0A:E2:D6:03`, driver 0.9.3 / Blog V4L, was inspected only:
57,888 bytes free PSRAM / largest block 32,768, 15,360,000 bytes of LoRa IQ buffers
(the 480,000-byte pre-roll was absent), no captured/decoded packets. Its antenna
was initially a 20 m antenna; the user subsequently fitted a 915 MHz mini whip.
The initial reception result is not a valid antenna-matched qualification.
No reset, flash, settings write or C6 update was performed on COM39.

## First candidate: 216fb392 (superseded)

Live filtering and linear interpolation now share a loop, retaining only the
final 500 ksps samples. Coefficients, intermediate quantization, interpolation
rounding, immutable input and protocol decoding remain unchanged. Maximum
preprocessing allocation falls from 7,680,000 to 4,000,000 bytes. Detailed replay
tracing retains its existing intermediate-buffer path and its existing memory
requirements; no improved trace-mode memory claim is made.

LoRa startup reserves that maximum live workspace before capture allocations.
Entering LoRa reclaims inactive holders; the existing ownership guards protect
active capture, decoder and export data. Safely reclaiming inactive LoRa buffers
also releases the new reservation. Workspace startup and decode failures have
explicit serial diagnostic stages. No DMA reserve or RTL driver change.

## Executed checks

- `python tests/lora_preprocess_memory_test.py`: PASS. Five generated lengths,
  through a maximum-size capture: byte-identical production preprocessing versus
  the existing two-stage path, immutable input, output-sized allocation budget,
  injected allocation failure and retry, maximum reservation and reservation
  failure/retry. Red run first failed because the compact implementation and
  reservation did not exist.
- `wsl python3 /mnt/f/Ai/OrcSDR-Temp/maps-integration-completion/tests/lora_decoder_startup_test.py`:
  PASS. The new reservation-failure case failed before the startup guard, then
  passed with it; existing queue/task cleanup and retry cases remain passing.
- `python .local/maps/check_lora_memory_syntax_20261025.py lora_native_decoder.cpp main.cpp`:
  both real-P4 syntax checks PASS.

Native linking passed (5,297,840-byte app; 993,616-byte factory headroom), and
COM17 app-only flash/non-app preservation and startup passed. However the 4 MB
reservation left only 1,095,316 bytes free before map allocation. This was
insufficient headroom to assume smooth map previews, so the candidate is
superseded rather than called a completed fix. C6 and DMA reserve unchanged.

## Revised candidate: reuse the owned automatic capture

Automatic jobs already take exclusive ownership of a swapped capture buffer.
They now filter and downsample behind the input already consumed, within that
buffer. Candidate, full fallback and CFO retries receive the same 500 ksps data;
candidate bounds are converted from the original sample rate. No extra live
capture-sized workspace is allocated. Manual diagnostic replay retains its
immutable raw samples and releases its temporary workspace after trace output.

The extended host test passes with all heap-cap allocations denied for owned
preprocessing, compares every processed byte with the old two-stage reference,
checks candidate/rate conversion into the decoder, and rejects diagnostic trace
requests on the mutating API. Address/undefined sanitizers pass. Both P4 source
syntax checks pass. Native rebuild and COM17 reception/map checks remain pending.

## Saved evidence

Raw logs are beside their invocation in the worktree root, outside Git:
`lora-blocker-{before,reclaim,lifecycle,restart,fresh,map-transition}-device-20261025.log`,
`lora-current-device-20261025.log`, `lora-cache-release-device-20261025.log`,
`com39-lora-readonly-20261025-01.log`, `lora-preprocess-memory-{red,green}-20261025.log`,
`lora-workspace-reserve-{red,green2}-20261025.log`,
`lora-startup-reserve-{red,green}-20261025.log`, `lora-memory-p4-syntax-20261025.log`.
First candidate build/flash/boot logs use `lora-memory-`; revised focused logs
use `lora-owned-`, including `lora-owned-sanitized-20261025.log`.
