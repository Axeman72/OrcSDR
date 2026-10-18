# OrcDial fresh-boot pairing memory investigation

Status: source/host checks pass; normal native build and COM39 device behavior pending.

## Observed failure

COM39, serial E8:F6:0A:E2:D6:03, reports 7,379 DMA bytes free, largest
7,168, with the pairing bridge stopped and `failure=low_memory`.
The old guard requires 20,480 DMA bytes. An unchanged COM17 also refused pairing.
Logs are saved in the worktree invocation directory:
`stability-com39-orcdial-memory-20261018.log`,
`stability-com17-comparison-20261018.log`.

The normal startup capture `stability-com39-boot-20261018.log` shows DMA free
72,867 at boot, 34,003 after Wi-Fi, 16,055 at ready, 9,263 after starting RTL,
then 7,623 after background network startup. The separate RTC heap still has
about 31 KiB free. Restarting alone does not satisfy the existing pairing guard.

## Narrow fix

- Allocate the existing 12 KiB security and 4 KiB relay task stacks from internal
  RTC RAM via ESP-IDF's WithCaps task APIs. Keep the same stack sizes and core
  affinity. Do not move an NVS-writing task onto PSRAM.
- Call the existing reclaim registry when an explicit Pair/Connect request needs
  to start the bridge. Each holder remains responsible for refusing to free
  buffers it is using.
- Check separate DMA and RTC budgets after reclaim: DMA free >=6 KiB and largest
  block >=4 KiB, RTC free >=16 KiB and largest block >=12 KiB. These are startup
  admission checks, not a proof of sustained runtime headroom.
- Refuse PSRAM queue allocation failures without falling back into the DMA pool.
- Replace restart advice with a truthful insufficient-memory message.

The local ESP-IDF 5.5.4 P4 memory layout marks RTC RAM internal and byte accessible,
without DMA capability. The candidate config enables RTC fast memory as heap.
FreeRTOS checks both internal/byte accessibility for task metadata and valid stack
memory. SDK WithCaps creation frees its TCB/stack if creation fails.
References: [heap capabilities](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32p4/api-reference/system/mem_alloc.html),
[task allocation APIs](https://docs.espressif.com/projects/esp-idf/en/latest/esp32p4/api-reference/system/freertos_additions.html).

## Tests actually run

- `cmake --build .local/orcdial-security --config Debug` then
  `ctest --test-dir .local/orcdial-security -C Debug --output-on-failure`:
  6/6 pass, including P4 stack placement, repeated task-start failures and PSRAM
  queue failures without an internal allocation fallback.
  Log: `orcdial-memory-host-final-20261018.log`.
- WSL `python3 tools/test_orcdial_memory.py`: PASS with ASan/UBSan. Executes the
  actual production preparation function; checks reclaim, fresh-boot-sized
  admission, insufficient DMA/RTC and fragmented blocks.
  Log: `orcdial-memory-policy-host-20261018.log`.
- Real P4 syntax check: PASS, `orcdial-memory-p4-syntax-20261018.log`.
- The new P4 placement test failed on the original allocation path. Its first
  Windows assert opened a debug dialog; that test process was stopped. The
  expected failing check now returns an error to stderr without a dialog.
  Log: `orcdial-memory-host-red-console-20261018.log`.

No COM17 or OrcDial firmware update is part of this change. A native link,
fresh COM39 recovery backup, app-only flash, measured pairing/RTC stack behavior,
Wi-Fi/RTL/SD coexistence and actual physical OrcDial delivery remain required.
