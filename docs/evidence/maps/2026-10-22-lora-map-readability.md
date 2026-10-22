# LoRa map readability correction

The user reported unreadable node names and sparse map detail. Approved bounded
design: double node-name text from size 1 to size 2, preserve OrcSDR selection
colors, and keep names inside the map without covering attribution or controls.

`lora_dashboard.cpp` now measures the size-2 label and shifts it inward near the
map edges. Marker coordinates retain the shared camera projection. The test
display records text size so the actual dashboard regression rejects size 1.
The test fails against old production drawing, then passes after the correction.

Commands from the integration root:

```text
wsl -d Ubuntu -- bash -lc 'cd /mnt/f/AI/OrcSDR-TEMP/maps-integration-completion && cmake --build .local/maps/labels-host --target orcsdr_lora_map_tests -j2 && ctest --test-dir .local/maps/labels-host -R "^lora_map_controls$" --output-on-failure'
wsl -d Ubuntu -- bash -lc 'cd /mnt/f/AI/OrcSDR-TEMP/maps-integration-completion && cmake --build .local/maps/map-serial-asan-20261021 --target orcsdr_lora_map_tests -j2 && ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 .local/maps/map-serial-asan-20261021/orcsdr_lora_map_tests'
python .local/maps/check_lora_map_syntax_20261021.py
```

Normal 1/1 PASS and sanitizer PASS, logs `lora-label-readable-{red,host,asan}-20261022.log`.
Initial P4 syntax fails on `std::max(int,long)` because native textWidth returns
int32_t (long on this target). Explicit bounded width conversion corrects it;
`lora-label-readable-p4-syntax-final-20261022.log` passes dashboard, adapter, main.
No native build or physical acceptance yet; slated for TEST-ONLY pilot firmware.

COM17's earlier native report explicitly uses Firmware world overview at stored
source zoom 4 versus camera zoom 9. Increasing camera zoom cannot invent map
detail. The approved 543 MB global regional-detail pack stores z0–8 and is the
next native source-quality gate, not proof of parity with Meshtastic/Google maps.
Street names are not a requirement. Worldwide visual quality remains open.
