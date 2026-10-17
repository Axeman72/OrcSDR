# Data & Maps inventory and migration

## Why Lane County appears

`catalog_sync.cpp:66-69` hard-codes five built-in identities/titles, including lane_county_map/LANE COUNTY MAP. Actual installed detection is separate (`refresh_installed`, line 163). COM39 reports that row uninstalled; COM17 reports it installed. A row is not proof of download.

The legacy path is `offline_map.hpp:10`, `/orcsdr/data/lane_county_map.idx`; header ORCMAP1 and bounded 640-segment/32-label cache are in offline_map.cpp:15-25,108. Legacy data remains discoverable and unchanged. Regional names remain legitimate metadata for actual regional packs.

PR #84's world ORCMAP1 asset was never merged. FT8's compiled world polygons are a separate firmware asset, not a shared installed regional pack. OrcMaps expects `/orcmaps/name.pmtiles` + `name.manifest.json` (+ optional checksum sidecar), PMTiles v3/MVT; no filename rename can convert formats.

## Cardinality and index safety

`catalog_sync.hpp:10-11` defines five built-ins and sixteen total slots. `settings_app.hpp:108` and drawing/touch loops expose only five. Slots 0..4 must retain existing IDs/command semantics. Dynamic p25_* profiles occupy later slots; arbitrary PMTiles IDs are currently not accepted by the signed catalog parser. Display all nonempty slots in five-row pages while sending original slot indices, never page-relative indices. Page changes and changed catalog identities clear destructive confirmations; invalid rows return no action.

The reviewed reconciliation branch supplies a small pure paging helper and targeted tests. Its independent contributions can be ported after review, retaining provenance. It does not supply PMTiles download/inventory.

## Truthful inventory states

Model separately: firmware/world overview; installed local pack; available signed offer; unavailable/no SD; malformed/partial/unknown legacy data; offline/unverified catalog. Show known identity, region/bounds, zoom, version, bytes, attribution/provenance and integrity level. Size pairing is not SHA integrity. Never label firmware text as installed data, or claim a catalog was verified without signature validation.

Preserve signature verification, declared hashes, staging `.part`, rollback `.bak`, version sidecars and existing P25 index gates. No removal/overwrite of user history or legacy maps. New pack formats need explicit destination/format validation, not a relaxation of the existing catalog security boundary.

Required tests: zero/one/five/six/sixteen/sparse entries, page bounds and original indices, install/remove confirmation reset, P25 entries, offline/no SD, legacy state and corrupt manifests. No device download/removal/migration is authorized by the initial read-only audit.

## Catalog implementation stage

Individually reviewed source from reconciliation commits through `a6ed3763` was selectively applied for the generic legacy title, 16-slot Settings snapshot, and pager UI. A tested Cursor now centralizes install/remove eligibility and confirmation state; page/catalog changes clear confirmation. Capacity is derived from `catalog::kPackCount` rather than a second numeric constant. No main.cpp changes, catalog parser changes, or device writes are involved.

MSVC C++17 `/W4 /WX /fsanitize=address /Zi /Od` test passed zero/one/five/six/sixteen/sparse entries, dynamic P25 slot, busy/offline gates, invalid indices, and removal confirmation/reset. Log is `maps-catalog-paging-msvc.log` in the invocation directory, with a reviewed copy in `docs/evidence/maps/`. A WSL attempt failed at VM connection establishment (HCS_E_CONNECTION_TIMEOUT), not a code test failure. Firmware compilation and device visual acceptance for this change remain separate gates.
