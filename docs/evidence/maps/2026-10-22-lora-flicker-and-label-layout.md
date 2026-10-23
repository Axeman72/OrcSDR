# LoRa map flicker and approved clustered-name layout

The user reports visible LoRa-map flicker after the row-batching experiment.
That experiment is retired and COM17 restored to `9ab05c04` while these source
corrections are prepared. Hardware rollback preserves NVS/SD/world/C6 and boots
the matching saved ELF. The defect is not attributed solely to the experiment.

Two independent redraw causes are reproduced in focused tests:

- `map_view.cpp` clears the entire map before calling the shared service, even
  when an opaque completed frame or cached preview immediately overwrites it.
  Pending without metadata can mean a preview-publication lock miss; it must
  preserve the visible image too. Only explicit unavailable/no-credit fallback
  clears the empty map area. Attribution/status behavior remains shared.
- `lora_dashboard::update()` redraws the map and selected-node panel for age ticks
  or unrelated radio counters. Map updates now depend on node identity, names,
  positions/selection or a new shared map revision. Header state updates remain
  independent. Camera actions and asynchronous completed frames still redraw.

The approved bounded node-name layout keeps every positioned marker, paints the
selected/followed name first, and tries ten nearby size-2 slots for other names.
It avoids accepted names, other markers, zoom controls and the map/credit bounds.
Names that cannot fit remain in Nodes; no nodes or saved data are deleted. It
uses a fixed 16-entry PSRAM record array, no heap allocation or new framebuffer.
UI and existing named serial camera controls share the same draw path.

Tests first fail on the original full-map clear and idle redraw
(`lora-map-flicker-red-20261022.log`, 0/2); the clustered-label regression then
fails overlapping names (`lora-cluster-label-red-20261022.log`). Corrections pass
the focused map-view/LoRa checks and sanitizer checks. Tests cover initial/pending/
ready/unavailable frames, telemetry ticks, publication, ownership, camera motion,
Follow identity and crowded-name priority/bounds/separation. Full-app flicker
acceptance is not inferred from these bounded checks.

Commands from the integration root:

```text
wsl -d Ubuntu -- bash -lc 'cd /mnt/f/AI/OrcSDR-TEMP/maps-integration-completion && cmake --build .local/maps/labels-host --target orcsdr_maps_tests orcsdr_map_view_tests orcsdr_lora_map_tests orcsdr_adsb_map_range_tests -j2 && ctest --test-dir .local/maps/labels-host -R "^(map_pipeline|map_view|lora_map_controls|adsb_map_range)$" --output-on-failure'
wsl -d Ubuntu -- bash -lc 'cd /mnt/f/AI/OrcSDR-TEMP/maps-integration-completion && cmake --build .local/maps/map-serial-asan-20261021 --target orcsdr_map_view_tests orcsdr_lora_map_tests -j2 && ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir .local/maps/map-serial-asan-20261021 -R "^(map_view|lora_map_controls)$" --output-on-failure'
python .local/maps/check_lora_map_syntax_20261021.py
```

Final logs `lora-flicker-final-{host,asan}-20261022.log` and
`lora-cluster-flicker-p4-20261022.log`. Native installation, physical flicker/name
acceptance and representative performance remain open. COM39 untouched.
