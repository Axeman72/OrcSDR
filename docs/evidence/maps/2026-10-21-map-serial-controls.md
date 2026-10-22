# Named map/setup serial controls

The user requires serial equivalents for OrcSDR user actions. This checkpoint
adds named map controls while preserving the existing UI ownership, location
storage, setup pause/restore and map installer paths. It does not claim full
application-wide serial parity.

Files: `ui/map_serial.cpp/.hpp` (bounded dispatcher), `ui/map_screen.cpp/.hpp`
(shared actions and state), `ui/map_service.cpp/.hpp` (presentation invalidation),
three-line registration/help changes in `ui/main.cpp`, native/host CMake lists,
`tests/maps/map_screen_tests.cpp` and command documentation.

## Behavior

- Read-only Help/Status/local inventory/download list remain available without
  authentication. Mutations require the existing authenticated session.
- Commands are bounded to 383 characters and fixed-size tokens; extra tokens,
  invalid E7 coordinates, invalid pan bounds and bad indices are rejected.
- Opening setup returns failure if reception cannot pause. Finish is allowed
  only at Complete and requires successful persistence. Cancel restores the
  existing return screen/reception behavior. Failed automatic progress writes
  no longer advance the screen without a saved checkpoint.
- Proposed picker coordinates are distinct from saved receiver coordinates.
  Save uses the same visible Saving acknowledgement and verified location hook
  as touch. Pan/zoom and pack selection use shared map-screen actions.
- Successful label writes invalidate shared presentation and refresh an active
  picker; preferences remain independent across all four consumers.
- Download commands use the same eligibility checks and installer APIs as UI.
  They cannot bypass trust, verification, activation or recovery.
- Inventory scratch is bounded and placed in PSRAM. There is no new private
  camera, receiver, download state machine or pairing key.

[User command reference](../../MAP_SERIAL_COMMANDS.md).

## Executed host checks

Invocation root: `F:/AI/OrcSDR-TEMP/maps-integration-completion`; Linux commands
run through WSL.

```text
cmake --build .local/maps/labels-host --target orcsdr_maps_tests -j2
.local/maps/labels-host/orcsdr_maps_tests /mnt/f/AI/OrcSDR-TEMP/orcmaps-quality-delivery/tests/fixtures/tiny-gzip.pmtiles /mnt/f/AI/OrcSDR-TEMP/orcmaps-quality-delivery/examples/m5stack-tab5/test-pack/springfield-97477.manifest.json .local/maps/fit-order-b59d478-20261020/orcmaps_world-before.bin
cmake -S apps/orcsdr-tab5/tests/maps -B .local/maps/map-serial-asan-20261021 -DORCMAPS_ROOT=/mnt/f/AI/OrcSDR-TEMP/orcmaps-quality-delivery -DCMAKE_BUILD_TYPE=Debug "-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer" "-DCMAKE_C_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer"
cmake --build .local/maps/map-serial-asan-20261021 --target orcsdr_maps_tests -j2
ASAN_OPTIONS=detect_leaks=1 .local/maps/map-serial-asan-20261021/orcsdr_maps_tests /mnt/f/AI/OrcSDR-TEMP/orcmaps-quality-delivery/tests/fixtures/tiny-gzip.pmtiles /mnt/f/AI/OrcSDR-TEMP/orcmaps-quality-delivery/examples/m5stack-tab5/test-pack/springfield-97477.manifest.json
```

Final optimized and ASan/UBSan runs PASS. The real map screen, navigation,
preferences, camera and location persistence execute against explicit device
fakes. Tests cover auth rejection without state changes; malformed/oversized
input; proposal versus saved point; pan/zoom; failed save staying open; Cancel;
successful Save; independent labels and failed label persistence; pack lookup;
setup pause failure/Back/Quick/Finish/failure/restore; automatic progress write
failure; download unconfigured/installed/index/auth gates and cancellation.
They do not execute actual HTTPS or emulate RF/USB scheduling.

Logs in the invocation root:
`map-serial-final-host-{build,test}-20261021.log`,
`map-serial-final-sanitizer-{build,test}-20261021.log`.
Earlier configure/build logs retain the same `map-serial-*` prefix.

## Native checkpoint

```text
python .local/maps/build_fit_order_20261020.py build map-serial-20261021
python .local/maps/build_fit_order_20261020.py build map-serial-corrected-20261021
```

The first build stopped on three `-Werror=misleading-indentation` findings in
the shared actions. The formatting correction preserves behavior. The second
build passes. Logs: `fit-order-native-map-serial-20261021-20261020.log` and
`fit-order-native-map-serial-corrected-20261021-20261020.log`.
Artifact inspection, COM17 installation and command acceptance remain open at
this source checkpoint. COM39 is untouched.

Remaining goal gates include actual global-pack/HTTPS delivery on device,
complete setup interruption/recovery, LoRa interaction, dashboard-specific
semantic controls (including ADS-B range), FT8 runtime, RF continuity and the
overall 33 ms target. No merge or publication occurred.
