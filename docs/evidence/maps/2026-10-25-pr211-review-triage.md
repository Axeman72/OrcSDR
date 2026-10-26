# PR211 release review continuation

This is a partial triage, not a clean whole-PR review. Continue on PR #211.

| Finding | Classification | Current disposition |
|---|---|---|
| Settings removal confirmation survives hidden catalog changes (4372201757) | FIX BEFORE MERGE | Source corrected; focused regression and P4 syntax pass; not installed on device yet |
| Map/setup value updates clear the full screen (4372201746) | FIX BEFORE MERGE | Source corrected; recording-display regressions and P4 syntax pass; native/physical checks pending |
| Unauthenticated saved receiver coordinates in new map status (4372201751) | FIX BEFORE MERGE | Source corrected in picker and LoRa status; authenticated/unauthenticated regressions and P4 syntax pass; device check pending |
| FT8 append retry can duplicate partially persisted rows (review 5645729155, outside diff, main.cpp:13505) | FIX BEFORE MERGE | Fixed-position retry source corrected; fault-injection and P4 syntax pass; native/device checks pending |

The other earlier findings are classified below from bounded current-source
inspection. They are not fixed or waived merely because the LoRa review passed. The
latest CodeRabbit pass on a9e04c49 added no inline comments on the LoRa memory
changes, but did raise the outside-diff append finding above.

## Catalog confirmation correction

Observe catalog changes regardless of the currently visible Settings section,
and invalidate the armed removal against that observation. Repainting remains
limited to the visible Data & Maps section. Unchanged snapshots preserve an
armed confirmation, so the fix does not make confirmation impossible.

`python tests/settings_catalog_invalidation_test.py` reproduced the hidden-change
bug before the source correction, then PASS. It exercises the production change
condition/observation and the real removal cursor: a different pack occupying an
old slot requires fresh confirmation, unchanged state preserves confirmation,
and a hidden update does not repaint the other section.
`map_catalog_paging_tests` PASS (standalone WSL g++ command in local evidence).
P4 syntax for settings_app.cpp PASS. No installed map was removed in testing.
Combined native/device validation remains pending alongside other review fixes.

## Grok bounded second opinion

Grok CLI, using the installed grok-review workflow, returned CLEAN for the
three-source-file memory correction b0542203..a9e04c49 (190 added / 20 removed
lines). The packet included ownership facts and the proposed cache test; no
repository, shell, write or web actions were permitted. It was not a whole-PR
review and does not establish that cache chunking fixes IQ losses.

Raw reports outside Git, beside invocation:
`grok-lora-release-review-20261025.log`,
`grok-lora-release-review-20261025.md`,
`settings-catalog-hidden-{red,green,final}-20261025.log`,
`settings-catalog-p4-syntax-20261025.log`.
Review snapshots: `.local/maps/pr211-source-findings-current-20261025.json`,
`.local/maps/pr211-lora-{formal-review,review-findings}-20261025.json`.

Radio loss under maps remains open after the failed cache-option acceptance
trial. The approved decoder/dongle release-runner extension, full fixture/source
audit, qualified RF matrix and tagged-package gates must still be completed.
No new PR, main merge, release tag or package publication occurred here.

## Map status and redraw corrections

Read-only map status retains active/readiness/zoom information for unpaired
peers. Exact saved/proposed coordinates and the LoRa followed-node ID require
the existing authenticated session. Both previous privacy regressions failed
first, then passed with the scoped output guards. This is not an app-wide
serial privacy audit.

Map-screen redraw now distinguishes entry/mode/step/editor transitions from
ordinary value changes. Ordinary setup, inventory and download changes replace
the content region rather than clearing the full screen; a valid picker map
remains visible during ordinary updates. Empty download taps and scrolls already
at their boundary do not repaint. No new framebuffer or heap buffer was added;
layout tracking is one bool and one uint32_t. Physical flicker acceptance is
still required. The recording stub now removes strings covered by rectangular
clears, so it checks disappearance of old text as well as new visible values.

Executed commands (worktree root, raw logs beside invocation):

- `wsl cmake --build /mnt/f/AI/OrcSDR-TEMP/maps-integration-completion/.local/maps/feature-replay-integration-20261023 --target orcsdr_maps_tests orcsdr_lora_map_tests orcsdr_setup_ui_tests -j 4`: PASS.
- `wsl ctest --test-dir /mnt/f/AI/OrcSDR-TEMP/maps-integration-completion/.local/maps/feature-replay-integration-20261023 -R map_pipeline --output-on-failure`: PASS, 1/1. Recording regressions cover repeated download messages, unchanged/animation ticks, inventory revision, setup network polling, no-op touches and entry/return behavior.
- Same CTest command with `-R lora_map_controls`: PASS, 1/1; unauthenticated status hides coordinates/node ID, authenticated output remains available.
- Same CTest command with `-R setup_ui`: PASS, 1/1.
- `python .local/maps/check_map_review_syntax_20261025.py map_screen.cpp map_serial.cpp lora_dashboard.cpp`: PASS for all three P4 compilation units.
- `git diff --check`: PASS.

Logs: `map-privacy-red-test2-20261025.log`, `lora-map-privacy-red-20261025.log`,
`map-redraw-{red,green3}-20261025.log`, `map-review-focused-build-20261025.log`,
`map-review-p4-syntax-20261025.log`. Device still runs the previously verified
5e79fd44 app; these source corrections await a combined native build/flash.

## FT8 CSV retry correction

The staged batch records its original file offset before its first write.
Retries reopen the same daily file read/write and seek to that offset instead
of appending the whole batch again. A partial new-file header is rewritten too.
The offset resets only after successful write, sync and close. Missing pending
files, status errors, or a file shorter than the saved offset fail closed.
No decoder changes, audio fixtures or new logging format are involved.

`python tests/ft8_log_retry_test.py` first reproduced corruption after a partial
header, then PASS against the production flush function. Cases cover partial
header/data writes, sync and close failures, additional staged rows before retry,
open/seek failures, unavailable file status and a missing pending file. The test
also verifies exact complete CSV bytes, not only return codes. Address/undefined
sanitizer run PASS. Main-file P4 syntax PASS.

Scope: in-session retries with the application as sole writer. RAM staging and
the retry offset are not journaled across power loss, so crash recovery and
concurrent external writers are not claimed. SD hardware validation remains
separate. Logs beside invocation: `ft8-log-retry-{red,final,sanitized}-20261025.log`
and `ft8-log-retry-p4-syntax-20261025.log`.

## Remaining earlier findings: bounded source verification

All are FIX BEFORE MERGE; none has been silently downgraded to OPTIONAL/STALE.

| ID | Current evidence | Next scoped check |
|---|---|---|
| 4372201699 | Temporary source-only `.coderabbit.yaml` filters still present | Review tests/shipping docs, then restore broad filters |
| 4372201703 | ADS-B zoom still calls full `redraw()` and reports settings_changed at range limits | Recording test for map-only zoom and no-op bounds |
| 4372201719 | FT8 globe still calls readPixel/drawPixel for every output pixel | Row-transfer equivalence/byte-order test before optimization |
| 4372201722 | FT8 pending publication leaves revision unchanged; update retries the map tab | Recording test for pending retries without static-panel clears |
| 4372201726 | Location worker uses WithCaps task allocation but ends with vTaskDelete | Task ownership/reaping fault tests against installed IDF 5.5.4 before changing deletion |
| 4372201727 | LoRa label toggle still calls draw_static | Recording test for bounded label/overlay update |
| 4372201733 | queue_local_rtl_listen returns immediately under setup_receiver_inhibit; later I/O deferral excludes setup_radio_paused | Lifecycle test for external navigation while setup is paused |
| 4372201738 | OrcDial TX task failure leaves null handle without setting initialization_retry_ms | Allocation-failure/backoff test; preserve paired-device behavior |
| 4372201742 | Download notice x318..412 intersects FT8 title/divider and LoRa divider | Header-geometry/recording checks for notice appearance and removal |
| 4372201745 | storage::used_bytes() returns zero, so capacity check is not real free space | FATFS capacity failure/full-card tests; no filling user's SD |
| 4372201755 | downloads::service executes synchronously ahead of map queue receive | Bounded concurrent transfer/render scheduling with shared-storage ownership tests |

The source inspection does not endorse the review's suggested implementation.
In particular, task self-deletion and transfer/render concurrency need their
own narrow ownership tests; do not replace them with an unverified one-line fix.
