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

One screen_controller owner; one radio_session owner. Maps may draw only on the owning surface or its off-screen target. Picker service is event driven; no touch polling loop. First migration should cover ADS-B's sprite target and variable viewport, followed by LoRa, then FT8/JS8 independently.

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
