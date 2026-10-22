# Approved TEST-ONLY GitHub map delivery pilot

The user explicitly approved the prepared `8c0e3b6c` pilot: 543 MB candidate
upload, TEST-ONLY P-256 signed catalog and COM17 installation/recovery tests.
Production signing, paid services, main merge, erasure and COM39 access remain
outside scope.

## Published endpoints

- [Candidate data prerelease](https://github.com/hardcoreerik/OrcMaps/releases/tag/sha256/16adde0b4f67734535db6871b89949e68c7e1dabf1c34d2bd7bd23842a9e5991)
- [TEST-ONLY signed catalog prerelease](https://github.com/hardcoreerik/OrcMaps/releases/tag/maps-catalog-test-20261022)

Both target engine `b59d47819cc3b81c1bbe50ed75b775275bb0218e`, are prereleases
and are excluded from latest. The existing `world-overview-1` draft is unchanged.
Release IDs: data `421233819`, catalog `421235072`.

Ruling: use tag `sha256/<full archive hash>`. Git accepts a pure 64-hex tag,
but GitHub refuses publishing it (`pre_receive ... 40 or 64 hex characters are
not allowed`). The first publication therefore fails HTTP 422 with both releases
still drafts. Prefixing with `sha256/` preserves the downloader's complete-hash
path-segment rule. The catalog is regenerated and signed again before either
release is published; no validator exception or security workaround is added.

Archive: 543,380,195 bytes, SHA-256
`16adde0b4f67734535db6871b89949e68c7e1dabf1c34d2bd7bd23842a9e5991`.
Actual producer hashes the full staged archive and invokes a newly built real
engine verifier; header-coverage contract passes with one installed, zero rejected.
Staging uses a local hard link, not a second 543 MB copy. No archive mutation.

Catalog revision `20261022214257`, generated `2026-10-22T21:42:57Z`, expires
`2026-11-22T21:42:57Z`. Final catalog 3,060 bytes, SHA-256
`102ebc795f930073c7b618c14e5789d873ef41aa926190fbf1489e303ce9569a`.
Detached signature 97 bytes, SHA-256
`5b371b59055702153530b7a7a2d8ba32273eaa32ac40e9171081771b09b87f49`.
TEST-ONLY public PEM 178 bytes, SHA-256
`7b899b1d10f3e98c348fd7a87c3bba32b4ae1ee833c2993122af3a7979118405`.
Private material is local, ignored and never uploaded or embedded in firmware.

The manifest, checksum and attribution/source/license notes accompany data.
This is the documented capped Protomaps/OSM/Natural Earth extract; upstream full
planet hash and exact Natural Earth revision remain unverified. Publication is
not worldwide visual acceptance or production capacity acceptance.

## Executed commands and evidence

Commands invoked from the OrcSDR integration root unless otherwise noted:

```text
python .local/maps/prepare_approved_pilot_20261022.py
wsl -d Ubuntu -- bash -lc 'cd /mnt/f/AI/OrcSDR-TEMP/maps-integration-completion && python3 /mnt/f/AI/OrcSDR-TEMP/orcmaps-quality-delivery/tools/pack-builder/build_map_catalog.py .local/maps/pilot-20261022/catalog-input-github-tag.json --pack-verifier /mnt/f/AI/OrcSDR-TEMP/orcmaps-quality-delivery/.local/pilot-pack-verifier-20261022/orcmap_pack_verify --private-key .local/maps/pilot-20261022/TEST-ONLY-keys/TEST-ONLY-private.pem --verify-public-key .local/maps/pilot-20261022/TEST-ONLY-public.pem --out .local/maps/pilot-20261022/signed-github-tag'
python .local/maps/upload_approved_pilot_20261022.py
python .local/maps/upload_pilot_catalog_20261022.py
python .local/maps/publish_corrected_pilot_20261022.py
python .local/maps/probe_published_pilot_20261022.py
python .local/maps/check_published_catalog_20261022.py
```

The Windows verifier rebuild first fails because CMake 4.0.3 does not support
cached generator Visual Studio 18 2026. Separate WSL Release verifier build
passes without source changes. Engine-worktree logs:
`pilot-pack-verifier-{build,linux-build}-20261022.log`.

Root logs: `pilot-{staging,catalog-producer,catalog-producer-github-tag,data-upload,catalog-upload,publication,publication-corrected,tag-correction}-20261022.log`.
GitHub inventory digests and sizes match local archives/metadata before publishing:
`pilot-verified-draft-{421233819,421235072}-20261022.json`.
Postpublication state: `pilot-releases-after-publication-20261022.json`.

Actual archive URL passes two 128-byte HTTP 206 ranges, exact Content-Range and
Content-Length, same strong ETag and If-Range, with verified HTTPS and bounded
redirects. Only 256 body bytes downloaded:
`pilot-published-range-probe-20261022.json` and `.log`.
Public catalog/signature/key downloaded within strict bounds match local bytes;
detached signature verifies using the downloaded public key:
`pilot-published-catalog-check-20261022.log`.

## Firmware and remaining device gates

Source `627c9d86` includes host-tested larger node names and `c6ab1833` stable
Follow ID. Installed COM17 remains `7c48e358` until app-only validation completes.
The TEST-ONLY build explicitly supplies saved SDKCONFIG, existing C6 3.0.6 image,
diagnostics OFF, pilot URL and public PEM; it does not establish production trust.

The prior backed-up NVS has no map-delivery namespace (trust floor zero), checked
without printing unrelated NVS values (`pilot-prior-trust-inventory-20261022.log`).
A fresh read is still required before catalog acceptance. Never erase/reset trust;
record the accepted test revision and require a compatible later production
revision/key-migration plan.

### COM17 pilot firmware and initial delivery checks

```text
python .local/maps/build_pilot_20261022.py build test-pilot-627c9d86-20261022
python .local/maps/inspect_test_pilot_20261022.py
python .local/maps/flash_test_pilot_20261022.py
python .local/maps/check_fresh_pilot_trust_20261022.py
python .local/maps/com17_label_bench.py test-pilot-catalog-check-plan-20261022.json test-pilot-catalog-check-device-20261022.log
python .local/maps/com17_label_bench.py test-pilot-first-cancel-plan-20261022.json test-pilot-first-cancel-device-20261022.log
python .local/maps/com17_label_bench.py test-pilot-resume-cancel-plan-20261022.json test-pilot-resume-cancel-device-20261022.log
```

Native build and inspection pass. App 5,157,440 bytes, partition headroom
1,134,016; SHA-256 `e435eef301ca9b284c2cf11907fff530a29ab84dc8cb4f0012f6d61738983127`.
ELF SHA-256 `5f93611dc95a78a6cc37acdac5f7863caf2e8931f08f150ead94f0c7c7ad586b`.
Exact pilot public key/URL, PSRAM placement, symbol gates, diagnostics OFF,
existing C6 3.0.6 input, dependency pin and full 40 KiB DMA reserve pass.
Artifact directory `.local/maps/pilot-627c9d86-20261022/` includes manifest and
fresh non-app backups. Previous app `7c48e358` verifies; app-only write and
bootloader/table/NVS/PHY/world/coredump preservation pass. COM39 untouched.

Fresh NVS backup contains no map-delivery namespace; trust floor zero, verified
without printing unrelated entries. No trust erase/reset. COM17 boots with
matching ELF hash and SD/Wi-Fi/RTL Ready. Native boot log is saved in its invocation
directory: `apps/orcsdr-tab5/test-pilot-boot-native-monitor-20261022.log`.

Setup opens with reception IDLE and receiver location unchanged at E7
`440628866,-1230160273`. Real HTTPS catalog check completes with phase 6,
configured=1, ready=1, count=1; the global product is recommended and not installed.
Given the compiled key and checked payload, the accepted revision is
`20261022214257`; later direct trust readback remains a separate persistence gate.

First install reaches 5,718,016 reported bytes, then cancellation finishes at
phase 3/busy 0, installed=0. Retry progresses to 14,385,152 bytes; a second
cancellation also finishes phase 3/busy 0, installed=0. No unexpected reset;
setup remains responsive to authenticated status/actions. These checks establish
bounded cancel/retry progress, not final archive integrity or activation.

Logs in root: `test-pilot-{artifact-inspection,verify-previous,backup-*,verify-backups,app-write,preservation,preflash-device,catalog-check-device,first-cancel-device,resume-cancel-device}-20261022.log`;
build log `fit-order-native-test-pilot-627c9d86-20261022-20261020.log`;
`pilot-fresh-trust-inventory-20261022.log`. Shared recording-display changes also
pass the real FT8 dashboard regression (`ft8-readable-stub-regression-20261022.log`).

Full transfer is started with a bounded adaptive observer:

```text
python .local/maps/com17_pilot_download_watch_20261022.py test-pilot-full-download-plan-20261022.json test-pilot-full-download-device-20261022.log
```

At this documentation checkpoint the observer is live; no completed-download
claim. It polls authenticated status every 30 seconds, records memory and fails
on a terminal non-success state. Do not restart solely because an observation
window expires: inspect the existing process/device first.

## Remaining gates

Complete 543 MB transfer, final hash/schema validation, activation/recovery,
insufficient space, corrupted data rejection, trust persistence, offline use,
physical larger-label acceptance and global-pack native quality remain open.
The full integration goal is not complete. TEST-ONLY catalog configuration is
local build input; do not accidentally inherit it into production/release builds.
