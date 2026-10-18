# OrcMaps consumers and integration design

Baseline: OrcSDR fba861e3, OrcMaps 14cdd41. Source is authoritative over stale README/STATUS text.

| Consumer | State/data owner | Current implementation/projection | Camera and overlays | Pack/format and evidence |
|---|---|---|---|---|
| ADS-B | ADS-B live/settings snapshots; canonical location copied through legacy settings | offline_map::draw_base on cached radar sprite; local equirectangular/NM basemap | receiver/range centered; aircraft use bearing/range polar placement | legacy `/orcsdr/data/lane_county_map.idx`, ORCMAP1; adsb_dashboard.cpp:106,120,1001 |
| LoRa | verified RF device/history snapshot | offline_map::draw_base/project | fixed 15 NM view centered on coordinate-bearing node; verified-coordinate markers | same legacy index; lora_dashboard.cpp:677,687,694 |
| FT8/FT4/JS8 | shared FT8 dashboard decoded grid/station snapshot | separate Natural Earth polygon array and equirectangular world projection | whole world by explicit existing preference; grid-square representative points | compiled asset, not PMTiles; ft8_dashboard.cpp:672-745; docs/ft8/CHANGELOG.md:134 |
| Airband | canonical receiver location through common Settings | no current map surface | location text/ATC selection; no independent camera | airband_dashboard.cpp:503 |
| Weather | no merged implementation | UNIMPLEMENTED on current main | PR #173 closed without merge | do not invent a current consumer |
| Location picker | no current implementation | old branch has prototype OrcMaps picker | blocking loop violates ownership/service requirement | historical setup_map_picker, not production |
| Setup wizard | no current implementation | old pure wizard/store model and blocking UI prototype | prototype skips welcome/network and auto-completes maps | not integrated on this baseline |

No other runtime map consumer found in the keyword/file audit. Satellite is not a current integrated map renderer in this baseline.

## Responsibility contract

OrcMaps owns PMTiles/MVT decoding, projection, viewport/camera, portable rendering and bounded scratch. OrcSDR owns receiver snapshots, canonical location persistence, signed downloads, screen transitions, and RF semantics. Current OrcMaps has no complete MapEngine/cache/overlay framework; build a thin shared map-session adapter over its public APIs, not a fork of its implementation.

Keep receiver location, map-pack selection, map-camera center, and approximate IP proposal separate. Projection used for basemap and markers must be identical after migration. Keep missing-coordinate aircraft/nodes unplotted, and label Maidenhead positions by their actual grid precision. Preserve FT8's whole-world preference.

One screen_controller owner; one radio_session owner. Maps may draw only on the owning surface or its off-screen target. Picker service is event driven; no touch polling loop. The first migration used LoRa; ADS-B followed in a separate commit. FT8/JS8 now call the shared whole-world compatibility adapter.

Do not copy old main.cpp, partitions or driver manifest. Consumer migration and P4 rendering remain UNTESTED until implemented and measured.

## Shared adapter foundation

`map_pipeline.*` now provides the common archive/reader/streaming-scratch/render path and E7-to-OrcMaps projection. It accepts a portable RenderTarget and cancellation/yield hooks; it contains no M5GFX, network, radio, touch or display ownership. Repeated wrapped tile placements decode once, then draw each placement. RenderStats distinguishes I/O, lookup, stream/decode and feature rendering; no P4 numbers are inferred from host timings.

`map_inventory.*` separately invokes OrcMaps discovery/pack resolution, preserves manifest metadata/attribution and explicit size-only integrity semantics, and rejects more than 16 manifests explicitly. The device filesystem adapter must also bound directory enumeration before allocation; host-only limits do not establish device safety. Signed catalog/P25 parsing is unchanged.

The candidate engine is reviewed PR #4 commit `70fea367909c74794bb69c7aed97e0f11f0a9387`, a descendant of v0.2.1, tested separately under MSVC AddressSanitizer. It is not claimed merged upstream or hardware-accepted. Dashboard/device integration is still pending.

### Reproduce foundation tests

From the repository root, with a checkout of the candidate engine:

```sh
cmake -S apps/orcsdr-tab5/tests/maps -B build-maps-tests -DORCMAPS_ROOT=/path/to/OrcMaps
cmake --build build-maps-tests --config RelWithDebInfo
ctest --test-dir build-maps-tests -C RelWithDebInfo --output-on-failure
```

On Windows this session used Visual Studio 18 2026, `/fsanitize=address`, and assertions explicitly enabled even in RelWithDebInfo. The test target passed projection/dimensions/antimeridian, repeatable pixels, cancellation, invalid archive/profile/zoom, 0/1/5/16 discovered packs, limit overflow, no SD, missing archive, mismatched size and malformed manifest cases. Log: `maps-pipeline-host.log` (invocation directory), reviewed copy in `docs/evidence/maps/`.

## Integrated callers (host-built; device gates pending)

| Caller | Shared entry point | Preserved semantics / remaining acceptance |
|---|---|---|
| LoRa | `maps::draw_region` / `maps::project(RegionalView)` | E7 verified positions only; follow camera stays separate from receiver location; ORCMAP1 fallback carries its matching projection |
| ADS-B | same region adapter into radar sprite | aircraft position and hit test use the basemap projection; missing coordinates and out-of-range tracks are not pinned to an invented edge location; radio list remains independent |
| FT8/FT4/JS8 | `maps::draw_world_compat` / `world_compat_project` | existing whole-globe equirectangular Natural Earth geometry centralized; full polar coverage and station-reported Maidenhead precision retained |
| Settings location / setup | `map_screen` + OrcMaps LocationPicker + shared worker | one proposal/confirmation flow; no network or SD prerequisite; manual polar E7 fallback |
| Airband | canonical receiver location, no new map | unchanged |
| Weather | absent on integration baseline | no implementation invented |

FT8 compatibility is deliberate, not a PMTiles engine claim: current OrcMaps provides
Mercator, which excludes polar grid locations, and its repeated-world placement would
change the owner's single whole-globe presentation. The existing compiled renderer and
projection now live in the shared map layer rather than each dashboard. A future engine
projection addition needs its own tests and upstream review before this path is retired.

`map_service` owns exactly one private PSRAM sprite and latest-request queue. It never
pushes a physical display from the worker; consumers request/push on the UI owner. Pan
queues/cancels before taking the frame lock, so the UI does not wait for rendering.
Memory reclaim invalidates the generation so the same viewport can be requested again.
Metadata rescans reopen the source/reader and publish a new inventory revision. The
frame cache and one PMTiles leaf cache are the actual caches; there is no invented
rendered-tile LRU, production MapEngine, or generic overlay framework.

Engine cleanup fix `c3cc09de3882149e03c7c00d7897c6882b324c6b` is under draft upstream
review at https://github.com/hardcoreerik/OrcMaps/pull/5, stacked on reviewed PR4. It
removes allocations during failed-tile cleanup. OrcSDR's prepared adapter catches
allocation failure and invalidates/reclaims the failed frame; host recovery tests pass.
The exception-enabled ESP32-P4 rebuild and all physical acceptance remain separate gates.

## Latest acceptance status — 2026-10-17

COM39 candidates `4971ee5b` and `e87c6d3c` failed before OrcSDR startup; none of the integrated
dashboard map consumers received a hardware acceptance pass. The original firmware
is restored and receiving LoRa. The placement adjustment reclaimed 592 internal BSS
bytes on the device but still failed boot. An opt-in startup diagnostic is prepared,
not linked or flashed. Consumer performance/visual claims
below remain host/source results until the COM39 gate passes. Full details and logs
are in `P4_PERFORMANCE_EVIDENCE.md`.
