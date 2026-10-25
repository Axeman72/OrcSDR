# LoRa capture memory recovery

COM17 reproduced a decoder workspace shortage after retaining 3,884,032 bytes of
map state. Only 586,112 bytes remained free; six captures failed before preamble
decoding. `RTL_MEM RECLAIM` released that map state and a subsequent capture
decoded with CRC valid without rebooting. COM39 independently had only 57,888
bytes free and incomplete capture buffers; it has only been inspected.

## Scope and implementation

1. Add a focused host regression comparing compact, streaming filter/resample
   output against the existing two-stage preprocessing, byte for byte. Include
   candidate/full lengths, input preservation, allocation failure and retry.
2. Keep filter coefficients, quantization, interpolation, chirp/FEC and protocol
   parsing unchanged. For live captures, retain only the final 500 ksps output,
   rather than allocating another full-size 960 ksps filtered capture. Preserve
   the existing detailed replay trace path.
3. Reserve the maximum live preprocessing workspace when starting LoRa. Reclaim
   inactive feature caches on the UI transition into LoRa; do not release active
   captures. Release the reserved workspace with safely reclaimable LoRa buffers.
   Keep task/fixed scratch allocations intact and keep startup retryable.
4. Expose workspace reservation failures through the existing named serial
   diagnostics. Do not advertise a functioning decoder after startup failure.
5. Run focused host equivalence/fault-injection and startup checks, then P4
   syntax/native build gates. Validate on COM17 using the existing recovery and
   app-only procedure, with map transitions and CRC-valid live reception.

No DMA reserve reduction, driver change, C6 update, settings erase, or COM39
flash is included. The user authorized memory repair and COM17 build/flash work.
Full decoder qualification and PR/release gates remain separate. No completion
claim until the corresponding host/native/device checks pass.
