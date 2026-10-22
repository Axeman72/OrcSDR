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

Native build/inspection, fresh trust inspection, P4 catalog acceptance, actual
543 MB transfer, cancel/resume, insufficient space, corruption/schema rejection,
activation/recovery, offline use and global-pack native quality remain open.
The full integration goal is not complete.
