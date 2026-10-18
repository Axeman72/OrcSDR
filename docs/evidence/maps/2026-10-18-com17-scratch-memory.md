# COM17 fresh-boot Pair refusal: decoder scratch placement

Source/host checkpoint; native build and device result pending.

The user reproduced insufficient memory to pair after rebooting COM17 on P4
source 6f001d86. The read-only capture `com17-pair-failure-20261018.log` confirms
DMA free 3,671, largest 3,200, bridge=0/failure=low_memory, and no held Map frame.
COM17 keeps C6 3.0.6; COM39 must retain C6 1.4.1. No C6 change is authorized
by this fix.

The existing ELF places `ft8_runtime::decode_slot(...)::chunk`, an 8,192-byte
int16 PCM staging array, at 0x4ff61dec in internal RAM. It is reserved at boot
even with FT8 inactive. Code inspection shows it is used only while the decoder
task copies samples from the existing PSRAM ring into the FT8/FT4/JS8 backend.
It is not an ISR or hardware DMA buffer. This is a directly identified avoidable
internal allocation, not an assertion that it explains every memory difference
between the two devices.

The narrow fix marks this existing zero-initialized static array
`EXT_RAM_BSS_ATTR`. It keeps the 4,096-sample size, copying, decoder algorithms,
task stacks, thread ownership, driver pin, C6 and pairing admission thresholds.
It does not add a buffer pool or change receiver/USB lifecycle.

`tools/check_tab5_ft8_scratch.py` checks the actual linked symbol's size and
containment in `.ext_ram.bss`, and is registered as an ALL target in native CMake
so an internal placement regression fails the build.

Tests run before native build:

- Placement gate on the existing ELF: expected FAIL, 8,192 bytes at 0x4ff61dec.
  Log `com17-scratch-placement-red-20261018.log`.
- Real P4 syntax check of ft8_runtime.cpp: PASS.
  Log `com17-scratch-p4-syntax-20261018.log`.
- WSL `bash tools/test-ft8-diagnostic.sh`: PASS for production, diagnostic and
  ASan/UBSan backend/pipeline variants and diagnostic argument handling.
  Log `com17-scratch-ft8-diagnostic-20261018.log`.

Next milestone: native link/placement gate, fresh COM17 recovery checkpoint,
app-only flash, measured post-boot DMA/RTC memory and Pair while receiving.
Preserve the verified original October 15 COM17 full-flash snapshot and the
current 6f001d86 recovery option. Then exercise FT8/JS8 scratch use and SD
readback; do not generalize a host pass into hardware/performance acceptance.
