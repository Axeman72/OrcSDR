# Production catalog activation

User explicitly approved production key creation/storage, signed publication,
map metadata promotion, tested builder push and production-configured COM17
validation. No main merge, firmware tag, erase, C6 update or COM39 access.

## Published

- [Production catalog](https://github.com/hardcoreerik/OrcMaps/releases/tag/maps-catalog-v1)
  is a published release, not TEST-ONLY. One global-regional product;
  `expires_at: null`, revision **20261025005000** (UTC generation; October 24 local).
- [World regional map](https://github.com/hardcoreerik/OrcMaps/releases/tag/sha256/16adde0b4f67734535db6871b89949e68c7e1dabf1c34d2bd7bd23842a9e5991)
  is promoted out of prerelease/TEST-ONLY metadata. The original 543,380,195-byte
  archive asset metadata is unchanged. The production manifest was added under
  `global-regional.production.manifest.json`; historical pilot assets remain.
- Builder commits through **5594d2adeed72aa13fae7e10b18f53169690394e** pushed to
  the existing development branch after 13 focused tests passed.

Catalog SHA-256: `126c46414b9a6acade3b328efab85cdd52ac692e88a902db9b8dd5693b25106c`.
Public PEM SHA-256: `d06ee971018586562dbe21cf44030a1d9e13d7b0fe65b6d54d3d7a4a4c7562dc`.
Signature asset SHA-256: `9bd641c55a6623577dabd333c8532d951e352a87e814b62cc0fdc19173705bf6`.
Direct public HTTPS reads match these signed local artifacts byte for byte.

## Key custody

Separate P-256 production private key is outside both repositories in the approved
user-private directory. ACL inheritance is removed; current user and SYSTEM have
access. A protected local backup copy exists. **Off-machine backup is not yet
established**: the local copy protects against file loss, not machine/disk loss.
No private key bytes appear in logs, Git, firmware or release assets. Public key
is committed as `apps/orcsdr-tab5/main/maps_production_public.pem`.

## Source and validation

`build-tab5-idf.ps1` now explicitly supplies the production URL/public key, including
when invoked by release packaging. Explicit overrides remain available; low-level
manual CMake builds without catalog inputs remain offline/unconfigured. This
prevents a stale pilot cache from silently becoming the standard script's input.
No M5Burner release package has been built or accepted in this step.

The signing script uses the tested OrcMaps assembler and actual local archive;
strict pack preflight and full archive hash validation pass. No archive transfer
or duplication of archive bytes was required (local hard link used). Signature
verifies with the production public key. Actual signed production payload passes
OrcSDR's host parser/session using the persisted pilot revision/hash; no trust-floor
reset. Test harness accepts an explicit expected revision instead of only fixture 1.
Six catalog configuration checks and PowerShell parser check pass.

Logs beside invocation in OrcSDR integration root:
`maps-production-signing-20261024.log`, `maps-production-publication-20261024.log`,
`maps-production-data-promotion-20261024.log`, `maps-production-public-https-20261024.log`,
`maps-production-signed-consumer-{build-,}20261024.log`,
`maps-production-catalog-config-20261024.log`.
Public-only reports and preflight output: `.local/maps/production-20261024/`.
OrcMaps logs: `maps-production-builder-activation-20261024.log`,
`maps-production-builder-push-20261024.log`.

Commands are retained in `.local/maps/prepare_production_catalog_20261024.py`,
`publish_production_catalog_20261024.py`, `promote_production_map_20261024.py`.
They fail rather than overwrite an existing key or staging directory.

Production-configured native build/link, COM17 migration/check and final firmware
release packaging are the next gates. Installed maps remain available offline
independently of catalog expiry; no performance or full-goal completion claimed.
