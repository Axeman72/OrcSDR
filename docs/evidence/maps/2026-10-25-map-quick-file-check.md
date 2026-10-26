# Quick map file check, replacing full archive scans

The user explicitly requested no full-file verification after map download:
"just simple file check after the end. not a full run". This supersedes earlier
device archive-hash acceptance requirements for this installation path.

The transfer and activation paths now check exact saved file size, readability,
PMTiles header/version/layout, bounded root-directory validity and declared map
coverage. They do not hash or scan tile payload. Repeated activation/recovery
calls use that same quick check rather than rereading the entire file. Existing
catalog signature/freshness checks, HTTPS, manifest identity, resume ETag/length,
small journal-record checksums, storage-close checks and transaction/rollback
behavior remain. Producer checksum sidecars remain useful for manual PC checks.

This deliberately cannot detect an unchanged-size corruption outside inspected
metadata. Tests explicitly demonstrate that payload-only corruption is not
rejected, while bad headers, wrong sizes and metadata/transaction faults remain
rejected. Do not call an installed archive hash-verified under this policy.
Installed status means the quick check and installation transaction succeeded.

No file-size read buffer or frame allocation is added. Existing caller-owned
transfer buffer remains. The PMTiles reader uses its existing bounded metadata
handling; there is no loop over all archive bytes. UI says "Checking map file"
instead of a full integrity/pass indicator. Existing serial metric names remain
compatible, with actual metadata read bytes and `hash_us=0`;
`ORCMAPS_VERIFY_PROFILE check=structure` distinguishes new measurements.

## Checks actually run

Run directory `F:\AI\OrcSDR-TEMP\maps-integration-completion`; raw logs outside Git.

- Modified `map_transfer_test` to require bounded metadata reads/no payload hash:
  old full-scan implementation failed the focused test (red).
- Built target `map_transfer_test` in `.local/maps/catalog-security-20261019`
  with CMake 4.3.3, then CTest Debug `-R '^map_transfer$' -V`: PASS 1/1.
  This exercises transfer, resume, cancellation, structural faults, activation,
  rollback and injected interruption scenarios. The final measured quick check
  read **153 bytes from a 2,097,275-byte fixture**, with zero hash time.
  Logs `map-quick-check-{build,test}-final-20261025.log`.
- Focused CMake `orcsdr_maps_tests` build and CTest `-R '^map_pipeline$'`:
  PASS 1/1, including updated visible progress and existing no-flicker checks.
  Logs `map-quick-check-ui-{build,test}-20261025.log`.
- `python .local/maps/check_map_quick_syntax_20261025.py map_transfer.cpp map_downloads.cpp map_screen.cpp`:
  all three real P4 syntax checks exit 0. Log
  `map-quick-check-p4-syntax-20261025.log`.

One initial green-build attempt exposed a missing profile member declaration;
it was corrected before the passing runs. A remaining old activation test
expected payload rehashing; it was changed to test header rejection, and an
explicit payload-corruption limitation test was added rather than claiming
full integrity. Earlier red/failed logs are retained.

Native linking and device timing are pending. COM39's ongoing download remains
untouched on its previously installed firmware, so it will still use the old
full-file checks. Do not reset it just to install this change mid-transfer.
The prepared clock shortcut will join the same next COM17 candidate.
