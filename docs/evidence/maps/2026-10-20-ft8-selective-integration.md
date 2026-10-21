# FT8 selective integration - October 20 source/host checkpoint

Cloud source reviewed: `origin/codex/ft8-orcmaps` at
`2e86835bb75720ab3286eac6e95c40d50e9b7c99`, based on `af877ed7`.
Imported the final source/test/build-registration delta only, not its branch
history or repeated historical plan changes. No main merge or cloud branch edit.

FT8 now obtains its world pixels from the shared map service using the embedded
firmware-world source. Its bounded 512-square Mercator frame is remapped into
its existing 824x420 whole-globe equirectangular view; polar caps are explicitly
grid-only. Grid/station/receiver coordinates use matching projection math.
Labels use the existing independent FT8 preference. Pending frames retain the
last coherent composition/receiver caption, with truthful status.
The two checked sprites need 1,216,448 B PSRAM, plus shared worker memory.
Allocation failure falls back to grid; sprites are freed on leave. The old
world-outline source is removed from firmware build registration, not deleted.

Scoped review found a publication race: the worker increments revision while
holding its mutex. If FT8 consumes that revision before draw returns pending,
no later tick retries the completed frame. Added a same-revision pending-to-ready
host regression; it failed on imported code. The correction consumes a revision
only after a nonpending result, retaining the mismatch for the next update.
The regression now passes without adding a new timer or periodic unavailable
retry. Other hardware memory/latency concerns remain unverified, not dismissed.

Commands from integration root:

- `wsl -e bash /mnt/f/AI/OrcSDR-TEMP/maps-integration-completion/.local/maps/test_ft8_scoped_20261020.sh`: optimized dashboard, complete projection pixel-centre sweep, ASan/UBSan dashboard PASS. Source dependencies are the same bounded dashboard/model/conditions/hunter/tuning/info/focus files used by cloud tests; no full DSP suite invoked.
- Initial test log `ft8-publication-race-red-20261020.log` records the expected dashboard assertion before correction. An initial CRLF shell-script invocation failed on shell options; corrected the local runner to LF before obtaining the behavioral failure.
- Corrected focused result `ft8-publication-race-test-20261020.log` PASS.
- Combined results `ft8-scoped-regression-20261020.log`: dashboard PASS, projection PASS, sanitized dashboard PASS.
- `python .local/maps/check_map_transfer_syntax_20261019.py ft8_dashboard.cpp ft8_map_source.cpp`: 2/2 actual P4 syntax PASS; `ft8-p4-syntax-20261020.log`.

Native linking, compositor speed/PSRAM reclaim and COM17 FT8/FT4/JS8 visual,
overlay and radio checks remain OPEN. Host adapter is stubbed; these tests do
not prove shared worker/native pixel conversion. The COM17 label firmware was
sealed before importing FT8 and does NOT contain these FT8 changes.
