# Shared setup and location implementation

Implemented in `ui/map_screen.*` and `ui/setup_flow.hpp`; hardware acceptance is pending.

## Flow and screen ownership

Welcome -> optional Network -> Receiver Location -> Offline Maps -> Complete.
Quick Start goes to Complete without changing receiver location, map choice, or tuning.
Back and Next / Skip change progress only. Cancel discards unconfirmed coordinates.
A location is saved only with USE LOCATION; a pack only with USE THIS MAP.
Previously explicitly confirmed changes remain saved when leaving the wizard.
Settings > System > RUN SETUP reruns it; Location > MAP PICKER uses the same picker;
Data & Maps > LOCAL MAPS uses the same bounded local inventory.

The screen uses a new `screens::Id::maps` value with the existing ownership rules.
No new touch loop, receiver owner, or blocking picker is introduced. Optional Network
opens existing Settings and returns to setup. The original Settings session is closed
before entering maps so its graphics state is restored correctly; returning opens the
original section with the original dashboard as its return target. Nested map actions
from optional Network retain that original return target.

## Canonical location and input

`receiver_location::Snapshot` remains canonical. Latitude/longitude remain E7; map
camera movement is a proposal, not a location write. Existing Settings and IP lookup
remain available; IP results still require the existing USE RESULT confirmation.
The map picker uses OrcMaps LocationPicker drag/recenter/zoom. Manual signed integer
E7 input works without Wi-Fi, SD, or a basemap; both coordinates must be entered on an
unconfigured device. Polar E7 values are retained as manual coordinates and explicitly
use a coordinate-only display outside Mercator coverage. Keyboard arrows focus controls,
Enter activates, Escape cancels, Ctrl+arrows pans, and +/- zooms.

Only map identity capacity expands (40 to 256 bytes) in canonical/settings/glue buffers.
Old strings and NVS keys are retained. Oversized identities or required credit are not
silently accepted as usable packs. Location writes retain existing label and map choice.
Confirmed coordinates now use one versioned 16-byte `loc_point` NVS blob. The three
legacy adsb_loc_set / adsb_lat_e7 / adsb_lon_e7 keys remain untouched as upgrade fallback
and old-firmware rollback state. Loading prefers a valid loc_point record; absent or
unreadable records fall back to legacy values. Unknown loc_point versions are never
overwritten by a new save. This does not claim that firmware rollback sees a location
saved only by the new firmware: older firmware sees its last legacy saved point.
No migration write occurs during loading. Picker, Settings/IP deferred persistence,
and serial location writers all use the same store so the new record cannot become
stale after another location editor is used. Pack choice remains map_pack.
Physical interrupted-save acceptance remains a device gate.

## Setup persistence and migration

One versioned UInt `maps_setup` contains progress/completion, never location or pack data.
It is loaded immediately after opening preferences, before legacy defaults are written.
Existing journal/workflow/location/map/network/display/pairing key presence classifies a
legacy installation, without reading private key contents. Missing setup_done alone never
classifies it as new. A blank device gets a checkpoint before defaults are written.
Current-version interrupted first run resumes its saved step. Unknown/corrupt/future
records are not overwritten; a rerun can still be cancelled but cannot silently replace
an unknown record. A rerun holds progress in RAM until Finish. No setup path clears NVS,
formats storage, erases SD files, or retunes the receiver.

## Verification and remaining gates

`setup_flow` host target passes for blank/legacy/resume/Back/Quick Start/future records,
and strict E7 boundaries, malformed input, overflow and unchanged output on rejection.
`map_pipeline` and optional `world_geometry` pass with AddressSanitizer. Logs are in
`maps-final-host.log` / `maps-consumers-host.log` in the invocation directory.
Native builds pass with the linked miniz gate. These are not physical UI acceptance.
COM17/COM39 confirm/cancel/rerun, Network return, keyboard/touch, no-SD behavior, actual
NVS persistence and active-RF operation are UNTESTED with this firmware.

A pre-flash blocker was found: native C++ exceptions are disabled, so OrcMaps vector
allocation failure can abort. The recovery patch now builds with a 1 KiB emergency reserve, passes host fault
injection, and is pinned to the tested allocation-free upstream cleanup. Build size
and provenance are recorded in P4_PERFORMANCE_EVIDENCE.md. Actual memory/radio
validation remains a device gate. No device was flashed.

## Pre-flash persistence review

The focused safety review identified the newly added picker callback's three-key save
as a mixed-coordinate risk during interrupted writes. `location_store.hpp` replaces
that path and the existing writer sites with one blob replacement. The host regression
fails on the extracted original save when the write budget is one, then passes with the
single record. It also covers failed replacement, complete update, legacy key retention,
unknown-schema rejection, invalid coordinates and a blank store without migration.
Logs: `maps-location-before.log` and `maps-location-after.log` in the invocation directory.
The bounded read-only review recheck reported no remaining actual defect; it did not
perform hardware tests. Serial coordinate entry also rejects non-finite values before
conversion to E7. No unrelated receiver or settings routing was refactored.

## Latest device status — 2026-10-17

The approved COM39 candidates `4971ee5b` and `e87c6d3c` failed before `app_main` during ESP-IDF's
internal/DMA reserve. The setup/picker was never exercised on hardware. NVS verified
unchanged before boot, and the previous firmware was restored successfully. This
does not validate canonical location migration or interrupted setup on the device.
The first map-state placement adjustment also failed boot. The approved diagnostic
now identifies an aligned-allocation boundary, reproduced in a 32-bit allocator
test. Four map metadata payloads, including the unchanged location Snapshot, are
prepared for PSRAM placement; full firmware build and device tests await approval. See
`P4_PERFORMANCE_EVIDENCE.md`. Existing host persistence results below remain valid.
