# Maps integration audit

Status: Phase 0/1 discovery; implementation is not complete. Date: 2026-10-17.

## Reproducible baseline

- OrcSDR base: `fba861e36970357e0a005c03d666250b09096cb6`.
- OrcMaps base: `14cdd418fe9101d56ea904eaa8c4da695a7fed9d`; v0.2.1 release `882093ee6ec133cc66afcfce7de4884e91f5d15f`.
- Isolated branches: `codex/maps-integration-completion` in both repositories.
- OrcSDR worktree: `F:/AI/OrcSDR-TEMP/maps-integration-completion`.
- OrcMaps worktree: `F:/AI/OrcSDR-TEMP/orcmaps-integration-completion`.
- Primary checkouts have unrelated untracked files, preserved. No repository AGENTS.md found in either base tree; session maintainer instructions apply.
- ESP-IDF build script pins 5.5.4; driver pin remains v0.9.3 `31a159df4f006a837d5041029bc6d1bc63520234`; Hosted 3.0.6. No baseline/dependency bump authorized.
- Existing factory partition is 6 MiB; NVS 0x9000/0x6000, PHY 0xf000, application 0x10000, coredump follows application. Preserve these offsets. Release size guard reads the partition table (`tools/release/build-m5burner.ps1:59`).

## History verified rather than assumed

- PR #23 merged as `efa9925a`: shared ORCMAP1 maps and catalog/location plumbing.
- PR #26 merged as `fc30c103`: dashboard/ownership work.
- PR #84 is CLOSED, not merged: its embedded world-coastline asset is absent from current main.
- PR #173 Weather foundation is CLOSED, not merged: no Weather dashboard exists in this base tree.
- Historical `claude/orcmaps-integration`: main ahead 340, branch ahead 15. It includes unrelated CB and older driver/partition changes. Do not merge it wholesale.
- Separate `origin/codex/orcmaps-reconciliation-2026-10-17` already offers pagination and region-neutral labels from this same base. Individual diff reviewed; no commits imported wholesale.
- OrcMaps PR #4 polygon rings remains open at `70fea367909c74794bb69c7aed97e0f11f0a9387`; historical CI success is not current hardware acceptance.

## Device discovery, read only

USB instance identities differ: COM17 ends `8&33159DA7&0&0000`; COM39 ends `9&1D2B9888&0&0000` (VID 303A/PID 1001). These identify USB paths, not a physical-label acceptance by the user.

COM17: uptime 143725176 ms, Airband receiving; catalog reports FAA/aviation/legacy map installed. Legacy map version 2026-08-21. COM39: uptime 14898322 ms, LoRa receiving; all five built-in catalog entries uninstalled. Both catalog states ready=0 (unverified/offline). This confirms a listing is distinct from installed state; it does not hash/validate the packs.

No authentication, stop, reset, flash, download, format, migration or SD write was issued. UI_DOC_LIST requires authentication and produced no version in this unauthenticated pass. Firmware SHA/version and NVS schema therefore remain UNTESTED live. Full SD listing is blocked during reception by sd_transfer_radio_busy; no radio pause was authorized in this read-only stage. IP-estimate status is not canonical receiver location.

Reviewed command outputs: `docs/evidence/maps/2026-10-17-com17-readonly.log` and `...com39-readonly.log`. Original logs remain in the command working directory.

## Audit trace

The bounded text trace is `.local/maps/source-trace.txt`, generated with rg across firmware/docs/tools/tests/workflows, excluding image/archive binaries. Key source anchors are in the consumer and migration documents. Large fixtures were not read into the audit.

## Baseline checks

- OrcMaps: CMake Debug host target built; ctest reported 1/1 target passed (contains the host cases).
- OrcSDR: `wsl -d Ubuntu -- python3 -m unittest discover -s .../tests -p test_data_catalog.py`: 3 tests passed. Initial native Windows attempt failed because openssl was absent from PATH, not a firmware failure.
- Fresh ESP-IDF native build passed (4,617,600-byte application); size/headroom and device acceptance limits are recorded in P4_PERFORMANCE_EVIDENCE.md.

## Published catalog check

The 3,036-byte published `catalog-v1.json` and 97-byte base64 signature were read on the host only. `openssl dgst -sha256 -verify .../catalog_public_key.pem -signature signature.der catalog-v1.json` reported `Verified OK`. It contains FAA aircraft, FAA aviation (legacy alias faa_aviation), and the legacy lane_county_map offer; no PMTiles offer is present. No pack payload was downloaded or installed.

## Remaining implementation

Full local-map inventory, canonical picker, safe wizard migration/rerun, shared OrcMaps rendering, consumer migrations, deterministic render tests and P4 measurements remain open. Every main.cpp change must be map/setup wiring only. No merges are authorized for this task.
