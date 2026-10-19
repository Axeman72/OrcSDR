# Map catalog/download UI and worker connection — 2026-10-19

## Scope and behavior

The existing map worker now handles bounded HTTPS catalog fetch, signature/trust
acceptance, archive transfer, verification, activation and recovery. No new task
or dashboard-owned renderer was added. Catalog entries include sizes, region,
zoom, recommendation and installed availability. Setup and Local Maps share the
Get Maps list with progress, Stop Transfer and explicit Retry. Returning to
Installed preserves the existing explicit Use This Map action. Location and radio
settings are not changed by an installation.

Rescan retries only durable pending activation; it never repeats an HTTP catalog
or archive operation. Failed/cancelled pending installs remain retryable offline.
The shared UI/worker eligibility functions prevent a pending intent from being
mistaken for active work, which previously hid Retry.

The catalog base HTTPS directory and public-only P-256 PEM are explicit CMake
inputs, ORCSDR_MAP_CATALOG_BASE_URL and ORCSDR_MAP_CATALOG_PUBLIC_KEY_FILE.
Both default empty. No production endpoint/key is selected; the screen states
that downloads are unconfigured and installed/embedded maps remain available.
The expected assets are maps-catalog-v1.json and maps-catalog-v1.sig. HTTPS,
trusted UTC, signature validation and persisted revision/hash are required
before exposing remote entries. Limits are 128 KiB JSON and 128 bytes signature,
including chunked replies. Config shape tests do not prove cryptographic validity.

Already-owned complete archives are fully reverified without redownloading.
A missing owned archive no longer appears installed; matching owned metadata
allows verified staged data to be renamed only into the missing archive path.
Existing files are never overwritten on this repair path. The durable install
intent makes a cut immediately after archive publication recoverable offline.

## Tests actually run

Invocation directory: F:/AI/OrcSDR-TEMP/maps-integration-completion.

- `cmake --build .local/maps/catalog-security-20261019 --config Debug --target map_transfer_test map_catalog_fetch_test map_download_actions_test`
- `ctest --test-dir .local/maps/catalog-security-20261019 -C Debug -R '^map_(transfer|download_actions|catalog_fetch|delivery_state|delivery_nvs)$' --output-on-failure`: **5/5 PASS**.
- `python apps/orcsdr-tab5/tests/maps/catalog_security/catalog_config_test.py`: **6/6 PASS**, default offline, configured public source, paired inputs, unsafe URL and private/oversized key rejection.
- `python .local/maps/check_map_transfer_syntax_20261019.py map_catalog_fetch.cpp map_downloads.cpp map_install.cpp map_activation.cpp map_service.cpp map_screen.cpp`: **6/6 PASS**, actual P4 SDK flags/headers.
- `ctest --test-dir .local/maps/catalog-security-20261019 -C Debug -R '^map_transfer$' --output-on-failure`: **1/1 PASS** after adding the repair-publication interruption scenario.

The missing-archive repair test failed before its correction. The already-owned
no-network test also failed before its fast path was implemented. New fetch and
eligibility tests initially failed to build because the implementations/APIs were
absent. Tests use simulated network/filesystem/NVS boundaries, not physical SD
power loss. Scoped read-only review identified the pending-Retry and missing
archive blockers, then confirmed their corrections with no further verified
blocker in that scope.

Logs are in the invocation directory:
`map-download-flow-{host-build,host-test,regressions}-20261019.log`,
`map-download-actions-{red,build,test}-20261019.log`,
`map-catalog-config-test-20261019.log`, `map-transfer-p4-syntax-20261019.log`,
`map-install-idempotent-{build-red,red}-20261019.log`,
`map-install-repair-{build-red,red,build}-20261019.log`, and
`map-install-repair-interruption-{build,test}-20261019.log`.

## Open gates

Native linking, PSRAM/stack/memory inspection and COM17 HTTPS/SD/NVS/UI/radio
validation remain unproven at this source checkpoint. COM17 remains on its last
accepted picker app; COM39 was not accessed. No global pack installation,
publication, production signing, source push or main merge occurred.

The wider goal still requires dashboard migration/label controls, global visual
acceptance, hosting approval and hardware delivery/fault testing. The 33 ms
interaction target remains unmet.
