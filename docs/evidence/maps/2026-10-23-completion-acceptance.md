# OrcMaps completion acceptance — October 23, 2026

Current source 4d4f85a3 on codex/maps-integration-completion. Last device-tested
app cb1b567a; no device observation has been performed in this October 23 scout.
OrcMaps pin b59d47819cc3b81c1bbe50ed75b775275bb0218e. This current matrix supersedes
stale unchecked entries in the historical plan without deleting their evidence.

| Area | Implementation | Recorded validation | Physical acceptance / remaining gate |
|---|---|---|---|
| Location picker/save | Implemented; verified explicit lat/lon persistence and feedback | Bounded COM17 save/return/reboot checks | Prior controls, readable text, saving feedback and dragging accepted; retain on final candidate |
| Shared map consumers and labels | LoRa, ADS-B and FT8 use shared service; independent preferences | Host and bounded COM17 checkpoints exist | Finish label persistence across all consumers, ADS-B aircraft/range and FT8 projection/whole-world/polar acceptance |
| LoRa map overlays | Bounded priority names, markers, centering and Follow implemented | Controlled text/NODEINFO and bounded position/Follow checks | Prior dragging/flicker accepted; Follow identity during history reorder remains unverified |
| Detail refill/read cache | 256 KiB reclaimable aligned-PSRAM cache; runtime construction corrected | Four cached pans median 4.10 s versus 6.23 s direct fallback; pixel equality, cancel/zoom/reclaim/SD checks | Latest candidate physical feel pending; decoder work varies, isolated multiplier not proven; further decode/draw work and explicit budgets open |
| Cached motion | Last-valid-frame preview and clipping implemented | Pan paint checks exist; representative continuous timing incomplete | Approximately 33 ms target remains open; paint-call timings are not full motion latency |
| Worldwide quality | Profile, hierarchy, rings/clipping and bounded labels implemented | Fixed worldwide host views recorded | Device boundaries/contrast/international-script and fallback acceptance still open |
| Setup | Wizard-owned methods and reception pause implemented | Bounded coordinate confirmation, IP proposal, exit/reception and graphics checks | Full first boot/rerun, credentials/address, Back/Skip/Cancel/recovery, audio and paused hotplug open |
| Delivery | Signed TEST-ONLY pilot and real installer connected | 543380195-byte HTTPS install/resume/activate/reboot/offline checks pass | Production key/hosting decisions unresolved; full/removed SD, corruption/interrupted activation/version replacement hardware checks open |
| Install latency | Safe verification implemented | Approximately 12-minute post-transfer verification/activation observed | Reduce or explicitly resolve without weakening verification |
| Serial parity | Named shared map/setup controls implemented | Focused source/host/device checks; latest restore uses RTL_LISTEN LORA | Reconcile stale coverage ledger and close unverified action/UI equivalence |
| SD/memory/radio | Aligned read cache, no new internal DMA pool; reclaim/reset paths implemented | Native 16 KiB retained-memory paused write/readback; active map reads; radio/log recovery pass | Final shared-candidate budgets, pressure/navigation checks open; historical failures preserved |
| Mainline reconciliation | Not yet applied to shared branch | Fetch and merge-tree simulation performed; five conflict files identified | Isolated resolution and scoped regressions pending; no merge into main authorized |

## October 23 scout

Tracked tree clean before this document; earlier untracked logs, binaries,
recovery images and the draft goal are preserved. No active native build/monitor
process was found among the named processes checked. The app remains a last-tested
state, not a claim of today's live hardware state.

Fetched origin/main is 629ba546. Integration is 48 commits behind main and 58
commits ahead of origin/codex/maps-integration-completion. Eight paths overlap:
main CMakeLists, ft8_dashboard.cpp, ft8_runtime.cpp, main.cpp,
screen_controller.cpp/.hpp, tests/ft8_dashboard_tests.cpp and tools/test-ft8.sh.

Exact simulation: git merge-tree --write-tree --name-only HEAD origin/main.
Expected conflict exit 1; log maps-mainline-reconciliation-scout-20261023.log in
the integration root. It creates an unreferenced tree without changing the index,
working files or branches. Five conflicts: FT8 chunk placement (equivalent),
Home paint ownership (retain Maps and Dead Air), screen ID/name registration
(retain both), independent map/clock test additions. Auto-merges still require
scoped checks, especially FT8 clock versus map-label header layout/hit routing.

Risk: medium for scoped isolated reconciliation; high for a whole-branch review.
Prepare a separate candidate under F:/AI/OrcSDR-TEMP. Preserve shared branch/main
refs. Run only affected FT8/maps/navigation checks first. Full branch review,
production decisions, erase and main merge retain their separate approval gates.

## Next milestone

Validate the isolated mainline candidate and reconcile this matrix with actual
results. Then define numeric motion/refill acceptance gates and profile repeated
decode/draw work on authorized COM17. Do not add an arbitrary overnight soak or
restore legacy geography. Production/publication choices remain decision-blocked,
not reasons to stop independent authorized implementation.
