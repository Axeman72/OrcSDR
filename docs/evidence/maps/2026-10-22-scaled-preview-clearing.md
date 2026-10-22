# Scaled preview: preserve the covered map interior

The global-pack COM17 baseline records scaled preview clear p50 7,685 us and
paint p50 62,642 us. Source inspection confirms `paint_preview()` clears the
entire map rectangle before `pushRotateZoom()`, unlike its existing pan path.
This also unnecessarily blanks covered pixels during zoom feedback.

The bounded correction extends exposed-strip clearing to scaled previews. It
uses M5GFX's pixel-centre mapping and a conservative one-pixel safety border for
float/fixed-point raster rounding. Only the guaranteed opaque interior skips
clearing; the subsequent image overwrites it. Uncovered regions are cleared once.
Clip restoration and the existing image transfer are unchanged. No framebuffer,
allocation, map-engine API, ownership, radio or saved-setting change is introduced.

Files: `apps/orcsdr-tab5/ui/map_preview.hpp` and
`apps/orcsdr-tab5/tests/maps/map_preview_tests.cpp`.

Commands from the integration root:

```text
wsl -d Ubuntu -- bash -lc 'cd /mnt/f/AI/OrcSDR-TEMP/maps-integration-completion && cmake --build .local/maps/labels-host --target orcsdr_map_preview_tests -j2 && ctest --test-dir .local/maps/labels-host -R "^map_preview$" --output-on-failure'
wsl -d Ubuntu -- bash -lc 'cd /mnt/f/AI/OrcSDR-TEMP/maps-integration-completion && cmake --build .local/maps/map-serial-asan-20261021 --target orcsdr_map_preview_tests -j2 && ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir .local/maps/map-serial-asan-20261021 -R "^map_preview$" --output-on-failure'
python .local/maps/check_picker_navigation_syntax.py
```

Red: the original full-clear path fails the zoom-in preserved-interior assertion.
Green: normal 1/1 and ASan/UBSan 1/1 pass, including 450 combinations of scale,
fractional displacement and clipping, plus existing pan/invalid-view cases.
The recording image samples coverage independently by inverse nearest-neighbour
coordinates; this is a host model, not execution of M5GFX's native panel raster.
Tests verify no stale uncovered pixels, no out-of-map writes, no double clears
and preserved interior during zoom-in. Five P4 syntax checks pass with existing
SDK header warnings. Logs: `global-scaled-preview-{red,host,asan,p4-syntax}-20261022.log`.

## Native candidate, not flashed

Source commit `9ab05c04dc81f7ecc692040f8949016c291837ec` builds and passes artifact
inspection. Commands from the integration root:

```text
python .local/maps/build_pilot_20261022.py build scaled-preview-20261022
python .local/maps/inspect_scaled_preview_20261022.py
```

Both exit zero. Build log: `fit-order-native-scaled-preview-20261022-20261020.log`;
inspection: `global-scaled-preview-artifact-inspection-20261022.log`. Artifact:
`.local/maps/scaled-preview-9ab05c04-20261022/`, including candidate app/ELF,
dependency lock and manifest. App 5,158,496 bytes, partition headroom 1,132,960;
app SHA-256 `8afa5c0da9b730154d8f876c4b5a7680059ffb4fa9fd0368c71a1bee864eeb91`;
ELF SHA-256 `f65038b6c5bc549dd56c9bba6dd1385bffb1fcdc25cb0603e0834819592c2f08`.

Inspection verifies PSRAM placement, unchanged 40 KiB DMA reserve, explicit saved
SDKCONFIG, diagnostics OFF, engine pin `b59d478`, no linked legacy map renderer,
shared serial dispatcher, preserved wizard source hashes and embedded C6 image
input 3.0.6. The build retains the approved TEST-ONLY pilot endpoint/public key;
it is not a production-release artifact. No device or C6 write occurred.

Scaled-edge appearance and performance comparison remain open. COM17's physical
availability is pending; installed app remains `ef53789f`. COM39 is untouched.
This does not claim the full 33 ms target: the previously measured scaled image
transfer is still the dominant cost.

## Pinned M5GFX scanline conformance

An optional CMake test now extracts the unchanged transform/inverse/scanline
bodies from the resolved M5GFX `LGFXBase.cpp` into the build directory and executes
them against the production preview helper and actual `pixelcopy_t`. It preserves
upstream attribution, edits no managed component, and copies no implementation
into product sources. Signature drift fails generation for review. The fake panel
records writes; this tests native raster coverage on a host, not physical timing.

`map_preview_native` passes 600 full-size picker/LoRa cases, including fractional
offsets, all supported power-of-two scales, intermediate scale, clipping, displaced
images and preserved interiors. The original full-clear helper fails its retained-
interior assertion (exit 134), recorded in `global-scaled-native-raster-red-20261022.log`.
Resolved source SHA-256:
`c9bff4ce17f1e2127bf708ad6ebc9d10b4ac7bfed557340811cfea61769f9918`.

Commands from the integration root:

```text
wsl -d Ubuntu -- bash -lc 'cd /mnt/f/AI/OrcSDR-TEMP/maps-integration-completion && cmake -S apps/orcsdr-tab5/tests/maps -B .local/maps/native-preview-host-20261022 -DORCMAPS_ROOT=/mnt/f/AI/OrcSDR-TEMP/orcmaps-quality-delivery -DM5GFX_ROOT=/mnt/f/AI/OrcSDR-TEMP/maps-integration-completion/apps/orcsdr-tab5/managed_components/m5stack__m5gfx && cmake --build .local/maps/native-preview-host-20261022 --target orcsdr_map_preview_native_tests orcsdr_map_preview_tests orcsdr_map_preview_format_tests -j2 && ctest --test-dir .local/maps/native-preview-host-20261022 -R "^map_preview(_native|_format)?$" --output-on-failure'
wsl -d Ubuntu -- bash -lc 'cd /mnt/f/AI/OrcSDR-TEMP/maps-integration-completion && cmake -S apps/orcsdr-tab5/tests/maps -B .local/maps/native-preview-asan-20261022 -DORCMAPS_ROOT=/mnt/f/AI/OrcSDR-TEMP/orcmaps-quality-delivery -DM5GFX_ROOT=/mnt/f/AI/OrcSDR-TEMP/maps-integration-completion/apps/orcsdr-tab5/managed_components/m5stack__m5gfx -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined" && cmake --build .local/maps/native-preview-asan-20261022 --target orcsdr_map_preview_native_tests -j2 && ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir .local/maps/native-preview-asan-20261022 -R "^map_preview_native$" --output-on-failure'
```

Normal preview/native-raster/byte-format checks pass 3/3; the native raster check
also passes ASan/UBSan 1/1. Logs `global-scaled-native-ctest-final-20261022.log` and
`global-scaled-native-asan-20261022.log`. Initial ad-hoc extraction needed explicit
UTF-8 decoding for the upstream source; corrected generator uses UTF-8. No full
test suite or additional firmware build was needed for this test-only addition.
