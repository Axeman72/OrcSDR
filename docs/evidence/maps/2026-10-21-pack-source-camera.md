# Keep installed map coverage at closer camera zoom

## Defect and shared adapter correction

The inventory previously required the camera zoom to be within the archive's
stored zoom range. A global z0–8 pack therefore ceased to be eligible at camera
zoom 9. A preferred regional pack outside the requested bounds also prevented
selection of another installed covering pack. Both could unnecessarily fall
back to the coarse embedded overview.

The correction is in OrcSDR's shared integration adapter, using the existing
OrcMaps resolver and camera APIs without engine/API/pack-format changes:

- Honor a preferred pack only when it covers the requested bounds and has
  suitable source detail. Otherwise consider other installed packs.
- Prefer coverage at the requested source zoom, then the highest stored level
  available below it. Existing OrcMaps priority/tie/coverage rules still apply.
- Adapt source zoom and screen tile size together, keeping the requested
  Mercator projection, center and screen scale exactly unchanged. Do not draw
  tiles with an undocumented mismatch between tile zoom and viewport zoom.
- Preserve logical camera zoom for labels; do not manufacture new map detail.
- Keep the embedded overview's existing one-level-coarser rendering policy.
- Add `ORC_MAP_SOURCE camera_zoom=... source_zoom=... title=...` to
  `RTL_MAP_STATUS`, making the selected detail observable over serial.

The same source checkpoint removes Oregon names/coordinates from documentation
demo state and neutralizes an airport service self-check example. Fresh receiver
state remains unconfigured, with empty location/map labels; the UI-document
demo is explicitly separate. Existing device locations and installed SD data
are not rewritten. The old filename remains recognized only for unsupported-file
reporting, and the retired signed-catalog identifier remains an internal parser
compatibility slot. Neither is a usable renderer or a shipping regional default.

## Tests executed

Invocation directory: `F:/AI/OrcSDR-TEMP/maps-integration-completion`.
Commands below run through WSL:

```text
cmake --build .local/maps/labels-host --target orcsdr_maps_tests -j2
ORCSDR_LABEL_TEST_ARCHIVE=/mnt/f/AI/OrcSDR-TEMP/orcmaps-quality-delivery/.local/global-assessment/global-z8-candidate.pmtiles .local/maps/labels-host/orcsdr_maps_tests /mnt/f/AI/OrcSDR-TEMP/orcmaps-quality-delivery/tests/fixtures/tiny-gzip.pmtiles /mnt/f/AI/OrcSDR-TEMP/orcmaps-quality-delivery/examples/m5stack-tab5/test-pack/springfield-97477.manifest.json .local/maps/fit-order-b59d478-20261020/orcmaps_world-before.bin
```

Before correction, the new inventory regression aborts at the assertion that an
installed pack remains eligible one camera level beyond its stored maximum.
Logs: `pack-overzoom-before-{build,test}-20261021.log`.

After correction: screen, pipeline, inventory and actual embedded-world checks
PASS. Inventory cases cover explicit preference, unavailable/out-of-coverage
preference, global/local source limits, minimum zoom, dateline and outside bounds.
Camera tests preserve projection at source z8 / camera z6, z8, z12 and z20,
including both sides of the dateline; invalid ranges/overflow are rejected.
Logs: `pack-overzoom-{build,test}-20261021.log`.

With the existing 543,380,195-byte global candidate, London, Tokyo and Nairobi
render at camera z9 and z12 using stored z8, with names, zero missing/failed
tiles and no bounded-stroke-capacity failures. Existing eight global reference
frames still pass. Logs: `pack-overzoom-real-{build,test}-20261021.log`.
These are bounded host reads of existing data, not a new download, full engine
suite or P4 timing measurement.

Native build/inspection, app installation and the actual SD global-pack device
test remain open at this source checkpoint. Production map delivery, full setup
acceptance, LoRa interaction, FT8 runtime and the overall 33 ms gate remain open.
