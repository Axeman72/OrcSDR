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

Native linking, scaled-edge appearance and performance comparison remain open.
COM17's physical availability is pending; no device mutation occurred for these
source checks. COM39 is untouched. This does not claim the full 33 ms target:
the previously measured scaled image transfer is still the dominant cost.
