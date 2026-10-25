# PR211 release review continuation

This is a partial triage, not a clean whole-PR review. Continue on PR #211.

| Finding | Classification | Current disposition |
|---|---|---|
| Settings removal confirmation survives hidden catalog changes (4372201757) | FIX BEFORE MERGE | Source corrected; focused regression and P4 syntax pass; not installed on device yet |
| Map/setup value updates clear the full screen (4372201746) | FIX BEFORE MERGE | Verified in current draw/poll paths; correction pending |
| Unauthenticated saved receiver coordinates in new map status (4372201751) | FIX BEFORE MERGE | Output verified; scoped privacy correction and tests pending |
| FT8 append retry can duplicate partially persisted rows (review 5645729155, outside diff, main.cpp:13505) | FIX BEFORE MERGE | Verified current branch code; correction/fault-injection tests pending |

Eleven other earlier inline findings still require verification/classification.
Do not label them stale, optional or fixed without checking current code. The
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
