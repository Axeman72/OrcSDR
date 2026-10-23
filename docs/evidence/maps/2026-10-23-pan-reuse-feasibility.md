# Detailed-pan reuse feasibility — October 23, 2026

Result: do not ship a shifted-frame-only detail completion or an unbounded
decoded-tile cache. These host probes do not close the COM17 two-second refill
gate and make no hardware speed claim. No firmware or device state changed.

## Shifted overlap is not a complete redraw

Compared the existing shared Pipeline's full render at the old camera with a
full render after an integer pan. Labels were disabled. Compared only pixels
present in both views after applying MakeFrameTransform; scale was exactly one
and translation was within 1e-6 of an integer. All assertions for archive open,
viewport selection and full renders succeeded.

Archive: existing global-z8-candidate.pmtiles, engine working head 7125ec1;
OrcSDR Pipeline source unchanged from 30a00ce1. Dimensions 842 by 330.
The zoom-9 view uses stored zoom-8 tiles through pack_source_view.

| Center / requested zoom | Pan 40,0 | Pan 0,20 | Pan -40,0 | Pan 0,-20 |
|---|---:|---:|---:|---:|
| 44.0628866,-123.0160273 / 9 | 7 | 7 | 29 | 8 |
| 35.7,139.7 / 8 | 2 | 7 | 1 | 12 |
| 0,179.99 / 5 | 0 | 0 | 0 | 0 |

Values are mismatching overlap pixels, from 261,020–264,660 comparisons per pan.
Eight of twelve cases differ. The localization log records sample coordinates
and bounding rectangles. Some differences are at old/new viewport edges; this
does not prove that repainting a fixed edge band repairs every case. No cause
is established by this comparison alone. Label-on layout was not tested.

Command, from the integration root:

```text
wsl -d Ubuntu -- bash .local/maps/run_pan_overlap_probe_20261023.sh > maps-pan-overlap-localization-20261023.log 2>&1
```

The executable returns 2 deliberately when any overlap differs. The outer
PowerShell/WSL invocation reported exit 1; inspect the actual result marker,
PAN_OVERLAP_RESULT mismatched_views=8, rather than interpreting that wrapper
status as a compilation failure. Initial comparison log:
maps-pan-overlap-probe-20261023.log. Localization source:
.local/maps/pan_overlap_probe_20261023.cpp, SHA-256
530e8bc69f90575316155886da06f3b6a958e7c2659d35b015870b39b53baf4c.

## Whole decoded geometry does not fit a safe default cache

Streamed the same six unique tiles through the existing no-text layer filter
and protomaps-v4 classifier, counting classified geometry without retaining it.
5,746 classified features contain 7,714 paths and 188,677 points. Point
coordinates alone require 1,509,416 bytes at sizeof(Point)=8. This excludes
path/feature containers, properties, allocation overhead, labels and cache index.
The restored COM17 sample had 1,698,828 bytes free PSRAM; these quantities do not
establish safe cache headroom. Counts are a host observation of this fixed view,
not a global maximum or a measured P4 cache allocation.

The first shell wrapper completed the probe but then failed on a trailing CRLF
line (command not found). Corrected that local wrapper to LF, then ran the
already-built executable directly; exit 0, without recompilation:

```text
wsl -d Ubuntu -- bash -c 'cd /mnt/f/AI/OrcSDR-TEMP/maps-integration-completion && .local/maps/decoded_tile_footprint_20261023 /mnt/f/AI/OrcSDR-TEMP/orcmaps-quality-delivery/.local/global-assessment/global-z8-candidate.pmtiles' > maps-decoded-tile-footprint-rerun-20261023.log 2>&1
```

Preserved first log: maps-decoded-tile-footprint-20261023.log. Source:
.local/maps/decoded_tile_footprint_20261023.cpp, SHA-256
dd08198900391b74f440db031e2248bac62943bdc1a15fcff16975cc23656f2e.
All logs and exploratory sources remain in the invocation worktree.

## Next performance gate

Choose a bounded reduction in repeated decode/draw work. Any retained geometry
or drawing-command representation needs an explicit byte cap, allocation-failure
fallback, source/camera identity and reclaim behavior. Any raster reuse needs
full-output equivalence across viewport edges and label changes. Prove those
properties before another native candidate, then measure the unchanged COM17
20-pan workload under reception. Current accepted firmware stays 30a00ce1;
p95 4.552282 seconds still fails the required two seconds.
