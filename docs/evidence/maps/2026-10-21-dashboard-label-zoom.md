# Dashboard label zoom and ADS-B range — October 21, 2026

COM17 (USB serial 30:ED:A0:E2:E6:95) runs the app built from `4b22585b`.
The reported Labels button defect was reproduced without resetting it:
authenticated taps at (110,276) changed only ADS-B's stored label preference
from 1 to 0 to 1, while every completed frame reported zero labels.
Log: `label-toggle-probe-20261021.log` in the OrcSDR worktree root.

The shared pipeline used the embedded archive's adapted source zoom (at most
4) for the city-name threshold (6). The correction passes the requested camera
zoom separately for label eligibility and priority, retaining the source camera
for tile lookup and projection. Legacy fallback also respects the threshold.
Labels OFF still supplies no label layout/text target. This does not add places
absent from the embedded overview or install the global candidate.

Focused checks, executed from the integration worktree root:

```text
cmake --build .local/maps/labels-host --target orcsdr_map_preferences_tests orcsdr_maps_tests -j2
.local/maps/labels-host/orcsdr_map_preferences_tests
.local/maps/labels-host/orcsdr_maps_tests /mnt/f/AI/OrcSDR-TEMP/orcmaps-quality-delivery/tests/fixtures/tiny-gzip.pmtiles /mnt/f/AI/OrcSDR-TEMP/orcmaps-quality-delivery/examples/m5stack-tab5/test-pack/springfield-97477.manifest.json .local/maps/fit-order-b59d478-20261020/orcmaps_world-before.bin
```

These Linux commands ran through WSL. Preferences PASS; map screen, world and
pipeline checks PASS. The actual backed-up COM17 world archive renders London
and Salem at logical zoom 6 / source zoom 4; coarse views remain unlabelled.
Logs: `label-toggle-host-build-20261021.log`,
`label-toggle-preferences-20261021.log`, `label-toggle-world-20261021.log`.
An additional explicit OFF render is included in the world regression;
its rerun is recorded separately in `label-toggle-off-*-20261021.log`.

The prepared ADS-B default is 10 NM on the short axis, approximately 47 statute
miles across the wide map. Existing valid saved ranges are preserved. Disabled
zoom endpoints are dimmed. Receiver centering continues to use saved lat/lon.

Native build, installation and corrected physical behavior remain unverified
at this source checkpoint. COM39 is untouched.

The earlier apparent hang was withdrawn by the user. The no-reset read-only
probe answered all five commands, with uptime 54,613,749 ms (about 15.2 hours),
20,637,308 bytes free PSRAM and an 8,704-byte largest DMA block. This is not a
controlled soak or a confirmed hang. Log:
`com17-hang-readonly-20261021-135603.log`.
