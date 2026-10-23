# Shared pipeline feature replay — October 23, 2026

Prepared consumer uses OrcMaps 2133547 (replay core plus overlapping arena
compaction). The tracked consumer pin/lock have not yet been updated at this
source checkpoint; this is not a linked firmware or a merge-ready dependency
state. Current COM17 remains accepted app 30a00ce1. No device access occurred.

## Implementation and limits

FeatureCache keeps at most 64 tile entries in caller-owned metadata. Before
rendering it evicts entries outside the visible tile set or with a different
place-layer mode, then compacts surviving records in memory order. Only a
successful stream and completed ordered drawing publish a tile. Failed append,
decode, render or cancellation cannot publish an incomplete prefix. Exhaustion
streams normally. Reopen/profile change invalidates entries; native source reset
also releases the arena. Reclaim detaches all source/frame allocations.

The native adapter supplies a single optional 768 KiB PSRAM arena. Metadata is
external BSS, runtime-constructed; Pipeline has only an extra pointer rather than
putting the entry array on a probe's stack or the internal heap. Allocation
requires room for the arena, 512 KiB remaining headroom and any missing preview.
This guard is a proposed budget, not hardware acceptance. The existing 40 KiB
DMA reserve, RTL driver, radio servicing, retained preview and SD read cache are
unchanged. Canvas resize releases replay before allocating mandatory storage.

## Tests actually run

Invocation worktree: F:/AI/OrcSDR-TEMP/maps-integration-completion.

```text
wsl -d Ubuntu -- bash -c 'cd /mnt/f/AI/OrcSDR-TEMP/maps-integration-completion && cmake --build .local/maps/feature-replay-integration-20261023 --target orcsdr_maps_tests orcsdr_map_feature_cache_tests -j2 && ORCSDR_LABEL_TEST_ARCHIVE=/mnt/f/AI/OrcSDR-TEMP/orcmaps-quality-delivery/.local/global-assessment/global-z8-candidate.pmtiles ctest --test-dir .local/maps/feature-replay-integration-20261023 -R "^(map_pipeline|map_feature_cache)$" --output-on-failure' > maps-feature-replay-integration-cancel-20261023.log 2>&1
python .local/maps/check_feature_replay_syntax_20261023.py
```

Both exit 0. CTest passes 2/2. Its preserved detailed output is
.local/maps/feature-replay-integration-20261023/Testing/Temporary/LastTest.log.
Ten reference-view pan/label comparisons match streaming pixels and measured
label-name sequences. Warm pans hit all six tiles, retaining 702,110 bytes.
Additional boundary-crossing pans, one-byte-arena streaming fallback, detach/
reclaim fallback, reopen invalidation and cancelled-replay recovery pass.
The text fixture records layout names; actual FreeType glyph equality was tested
separately against the engine core, not claimed by this recording target.

The focused cache test also passes ASan/UBSan/leak checking: command and output
maps-feature-cache-sanitized-20261023.log. It tests overlapping compaction,
failed append, cancelled publication, surviving-label integrity and mode eviction.

P4 syntax log: maps-feature-replay-p4-syntax-20261023.log. Pipeline, map_service
and feature_replay.cpp pass using explicitly selected local engine headers and
the existing compiler database. This is not a resolved native link or a memory
section check. The full nine-program OrcMaps run/publication is separately
approved and running at this documentation checkpoint.

## Observability and remaining gate

RTL_MAP STATUS now includes ORC_MAP_REPLAY with capacity_bytes, retained_bytes,
tile_hits and fallbacks for the last frame. Held/reclaimed memory includes the
arena. No new user action or authentication path is introduced.

Resolve approved producer publication, regenerate the pin/lock, build and inspect
P4 section placement/size, then verify COM17 identity/recovery and perform the
already-authorized app-only update. Measure the unchanged 20-pan workload under
reception, cancellation, low-memory fallback/reclaim, SD and radio continuity.
Do not infer two-second acceptance or a speed multiplier from these host results.
