# Setup and local-map usability candidate

User-approved scope: larger setup text, scrollable local-map list, understandable
map descriptions/details, location-method choices, easier Use Location, removal
of ADS-B radar grid and opposite fresh-install landscape orientation.

Prepared source changes:
- map_screen.cpp: four visible selectable map rows with swipe and Up/Down scrolling,
  readable title/coverage/zoom/status, separate Details, existing selection hook.
- Setup location choices reuse existing Settings network/address/coordinate entry
  through a small action hook; map selection remains offline-capable. Saved location
  appears on return. Internet lookup behavior and providers are unchanged.
- Use Location has a 60-pixel-high matching draw/hit rectangle, accepts the center
  without a prior pan, and reports save failures at text size 2. Back from setup's
  picker returns to method selection. No evidence yet of physical coordinate offsets.
- ADS-B removes four rings and horizontal/vertical radar lines. Aircraft and cardinal
  labels remain. Projection, range filtering and receiver operation are unchanged.
- Fresh orientation defaults change 1 to 3 at early startup, state load and Settings
  initialization; stored valid rotation values are preserved.

Tests: Linux focused CTest setup_flow, location_store, map_pipeline all PASS (3/3),
including actual picker/Settings code, scroll-to-fifth-map selection, current-center
save and existing changed-point/save-failure/cancel/pan/zoom tests.
Logs: setup-list-linux-build-check-20261018.log and
setup-list-linux-test-check-20261018.log. Earlier Windows ASan-host tests stalled;
only their own test/CTest processes were stopped. Initial Linux test exposed an
anonymous-type inline M5 stub problem; naming FakeM5 shares the display correctly.
Initial fixture compile error and all prior logs retained.

Native build, hardware touch/readability, selected Springfield dashboard rendering,
radio continuity and orientation are pending. No hardware acceptance claimed yet.
Next: one native build and COM17 app-only flash with a fresh settings backup;
leave C6, SD and COM39 unchanged. Preserve the user's current setup progress.
