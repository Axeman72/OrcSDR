# LoRa worker startup with map memory retained

## Observed failure

COM17 (USB `30:ED:A0:E2:E6:95`), app `f61bdad5`, reported
`RTL_LORA_NATIVE_INIT_FAIL stage=task` on entry to LoRa. The driver still
streamed at about 955 ksps, with zero reported driver overruns/drops, but the
native decoder reported `ready=false busy=true` and history remained empty.
This is not evidence of successful decoding or of an antenna problem.

The user initially identified a powered GA-800 indoors beside a window, then
confirmed replacing it with a 915 MHz antenna before the controlled RF check.
COM24 (`44:1B:F6:6F:81:BC`) identifies as HELTEC_V4, node `!aa745cf5`, US
LongFast, automatic slot, no frequency override. COM17 uses US slot 20,
906.875 MHz. NODEINFO packet `3286217580` and text packet `711768941` were
submitted to the Heltec with hop limit zero and no acknowledgment request.
Neither was decoded during the bounded check. Submission is not proof of
over-air transmission. No radio settings, position settings or keys were changed.
Position sharing is disabled (`position_precision=0`) and no usable position
was returned, so no position packet was submitted.

## Narrow correction

The decoder requested a 12 KiB internal task stack while the observed largest
internal block was 7 KiB. Use the same PSRAM stack API already used by the
FT8, ADS-B and DSP workers, preserving stack size, priority and core. On startup
failure, delete the queue and clear handles. Automatic capture admission also
requires published decoder readiness, preventing enqueue before startup succeeds.
A queue left behind after failed task creation previously accepted one capture
and set busy with no worker to consume it.

Files: `apps/orcsdr-tab5/ui/main.cpp`,
`tests/lora_decoder_startup_test.py`, this evidence note.
No decoder algorithm, pack, SD file or receiver-location change.

## Checks completed before native build

From the integration worktree root:

```powershell
wsl -d Ubuntu -- python3 /mnt/f/AI/OrcSDR-TEMP/maps-integration-completion/tests/lora_decoder_startup_test.py
python .local/maps/check_setup_confirm_syntax_20261022.py
```

The startup harness compiles the actual production startup function with
fault-injected queue/task/workspace allocation. The old source fails the
post-task-failure cleanup assertion (`lora-startup-red-20261022.log`). Corrected
source passes initialization/workspace/queue/task failure, successful retry,
idempotence and PSRAM stack arguments (`lora-startup-host-final-20261022.log`).
The real P4 compiler syntax checks pass for main and setup UI
(`lora-startup-p4-syntax-20261022.log`). These do not prove hardware startup.

Raw device evidence: `lora-map-position-inventory-device-20261022.log`,
`lora-antenna-swap-pause-device-20261022.log`,
`lora-915-resume-device-20261022.log`,
`lora-915-packet-check-device-20261022.log`,
`lora-init-failure-stop-device-20261022.log`.
Transmitter inventory/submission: `heltec-map-test-{inventory,send}-20261022.log`.
All logs are saved in the invocation worktree root. COM17 reception was stopped
after the failed check. COM39 was not accessed.

## Remaining gates

Native linking, app-only preservation/boot, actual controlled RF decoding,
positioned-node overlays/follow and retained-map SD checks remain unverified for
this correction. A separate source scout also found index-based node selection
against a history reordered by reception; stable Follow identity still needs a
focused reproduction. No correction or completion claim is made for that issue.
