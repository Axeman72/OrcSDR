# Retire personal regional-map defaults and legacy rendering

The user identified a Lane County/Springfield-looking map flashing during ADS-B
zoom and clarified the shipping requirement: OrcSDR must be a worldwide product,
without the developer's personal regional setup as its default experience.
This supersedes the earlier requirement to retain the ORCMAP1 renderer.
Existing user files and saved receiver settings must still be preserved.

## Confirmed cause and source changes

`map_view.cpp` previously rendered ORCMAP1 whenever the asynchronous OrcMaps
frame was pending. This overwrote the shared cached preview and temporarily
changed both labels and overlay projection. It also explicitly bypassed OrcMaps
when the saved pack identity was `lane_county_map`.

- Remove `offline_map.cpp/.hpp`, its CMake source entry, startup/catalog reload
  calls and its boot self-check dependency.
- Regional dashboards always use the OrcMaps camera/service. Pending frames
  use its transformed cached preview, including the preview's original credit.
  No ORCMAP1 draw or projection path remains.
- Store preview metadata in PSRAM, under the same publication lock as its
  pixels and camera. No added internal/DMA buffer is required.
- Retain an existing old file only as an unsupported, unselectable inventory
  entry. Do not manufacture that entry on a clean device. Do not delete files.
- Retire the historical regional-map slot from public data-catalog state,
  serial help and install/remove operations. Preserve internal signed-catalog
  indices/parsing so older signed catalogs and other data packs still work.
- Replace the receiver-location boot self-check's personal place strings and
  coordinates with neutral test values. The self-check restores saved state.

This does not remove user-installed regional PMTiles packs. Such packs are valid
OrcMaps data; switching to them must follow coverage/selection policy, never
temporary legacy fallback. This does not claim an application-wide product
defaults audit. Remaining UI-document demo content includes an Oregon example;
it is not a first-boot location, and needs separate cleanup before shipping.

## Focused regression

Invocation directory: `F:/AI/OrcSDR-TEMP/maps-integration-completion`.
Linux commands run through WSL:

```text
cmake -S apps/orcsdr-tab5/tests/maps -B .local/maps/labels-host
cmake --build .local/maps/labels-host --target orcsdr_map_view_tests orcsdr_map_preview_tests orcsdr_maps_tests -j2
.local/maps/labels-host/orcsdr_map_view_tests
.local/maps/labels-host/orcsdr_map_preview_tests
.local/maps/labels-host/orcsdr_maps_tests /mnt/f/AI/OrcSDR-TEMP/orcmaps-quality-delivery/tests/fixtures/tiny-gzip.pmtiles /mnt/f/AI/OrcSDR-TEMP/orcmaps-quality-delivery/examples/m5stack-tab5/test-pack/springfield-97477.manifest.json .local/maps/fit-order-b59d478-20261020/orcmaps_world-before.bin
```

Before correction, the new production-adapter test failed to link because it
still required the legacy renderer's availability, drawing and projection
functions (`map-view-legacy-link-before-20261021.log`). After removal it links
without that renderer and PASSes pending/ready/unavailable cases, stale legacy
identity routing, overlay origin and pending attribution. Existing preview and
actual embedded-world/pipeline/screen checks also pass.
Logs: `map-view-orcmaps-only-*-20261021.log`,
`map-view-preview-test-20261021.log`, `map-view-world-test-20261021.log`.
The rerun with the final neutral self-check is recorded in
`map-view-retirement-{host-build,host-test,world-test}-20261021.log`.

Native build, binary symbol removal, COM17 flash/preservation, serial catalog
behavior and physical zoom acceptance are pending at this source checkpoint.
COM39 is untouched. Next milestone: verify a single OrcMaps transition on COM17,
then continue regional/global coverage and the broader product-default audit.
