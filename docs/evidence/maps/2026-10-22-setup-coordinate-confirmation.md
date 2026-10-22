# Setup coordinate review and serial save parity — October 22, 2026

The self-contained coordinate input saved immediately after longitude entry,
although the screen instructed users to confirm before saving. Its review view
did not show the entered coordinates. The correction retains a proposal,
displays latitude/longitude at text size 4, and writes only after SAVE LOCATION.
Back can abandon the proposal without replacing the saved receiver location.
Failed saves keep the proposal and retry button; existing Saving feedback stays.

Related serial audit: `RTL_ADSB_LOCATION <latitude> <longitude>` lacked the
authentication check present on other location mutation commands and duplicated
persistence/update logic. It now requires authentication and calls the same
verified `save_map_receiver_location` hook used by the picker and wizard.
Arguments/ranges, result names and the ADS-B refresh behavior are retained.
This is an explicit save command, not a coordinate-proposal command.

Changed source: `apps/orcsdr-tab5/ui/setup_ui.cpp`, `ui/main.cpp` and
`tests/maps/setup_ui_tests.cpp`. No network, receiver, DSP or SD implementation
is changed. No production catalog/key or hosting configuration is introduced.

## Focused evidence

The new no-save-before-confirm assertion fails against the original code at
`setup_ui_tests.cpp:58`; saved log:
`setup-coordinate-confirm-red-test-20261022.log`. Source correction then passes:

```text
wsl bash -lc 'cd /mnt/f/AI/OrcSDR-TEMP/maps-integration-completion && cmake --build .local/maps/labels-host --target orcsdr_setup_ui_tests orcsdr_setup_tests -j2 > setup-coordinate-confirm-host-build-corrected-20261022.log 2>&1 && ctest --test-dir .local/maps/labels-host -R "^setup_(ui|flow)$" --output-on-failure > setup-coordinate-confirm-host-test-20261022.log 2>&1'
wsl bash -lc 'cd /mnt/f/AI/OrcSDR-TEMP/maps-integration-completion && cmake --build .local/maps/map-serial-asan-20261021 --target orcsdr_setup_ui_tests orcsdr_setup_tests -j2 > setup-coordinate-confirm-asan-build-20261022.log 2>&1 && ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir .local/maps/map-serial-asan-20261021 -R "^setup_(ui|flow)$" --output-on-failure > setup-coordinate-confirm-asan-test-20261022.log 2>&1'
wsl bash -lc 'cd /mnt/f/AI/OrcSDR-TEMP/maps-integration-completion && cmake --build .local/maps/labels-host --target orcsdr_maps_tests -j2 > setup-coordinate-confirm-map-build-20261022.log 2>&1 && ctest --test-dir .local/maps/labels-host -R "^map_pipeline$" --output-on-failure > setup-coordinate-confirm-map-test-20261022.log 2>&1'
python .local/maps/check_setup_confirm_syntax_20261022.py
```

Results: setup UI/flow **2/2 PASS** normally and with ASan/UBSan; map pipeline
**1/1 PASS**; P4 syntax **2/2 PASS** (`setup_ui.cpp`, `main.cpp`). The UI test
covers no save on longitude acceptance, visible review values, failed save/retry
and cancellation preserving the old point. Initial host build used a mistaken
target name `orcsdr_setup_flow_tests`; that invocation failed before CTest and
was corrected to the actual `orcsdr_setup_tests` target. Its failed log is kept.

All logs are in the worktree root. P4 syntax log:
`setup-coordinate-confirm-p4-syntax-with-serial-20261022.log`.
These host tests stub storage and radio; the serial command authentication
branch is compiled for P4 but not executed by them. Native link and COM17
authentication/UI/save/cancel checks remain next. Full first-boot, all location
methods and interrupted-setup acceptance are not closed by this correction.
