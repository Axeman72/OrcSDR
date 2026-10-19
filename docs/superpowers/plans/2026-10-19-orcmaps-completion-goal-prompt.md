# Continuation goal prompt — OrcMaps / OrcSDR

Copy the prompt below into the continuation chat. It describes the remaining
goal; it does not grant blanket approval for costly or destructive operations.

---

Continue the existing OrcMaps / OrcSDR map-quality and delivery work until the
required acceptance gates pass and the reviewed OrcSDR integration is merged
with my explicit approval. Do not restart the project or stop after a proposal.

Use this plan as the checklist and update it as evidence changes:
F:/AI/OrcSDR-TEMP/maps-integration-completion/docs/superpowers/plans/2026-10-18-orcmaps-orcsdr-quality-and-delivery.md

Start with a bounded scout of repository instructions, current status, relevant
commits and outstanding gates. Verify this handoff rather than assuming it is
still current:

- OrcSDR worktree: F:/AI/OrcSDR-TEMP/maps-integration-completion,
  branch codex/maps-integration-completion, source checkpoint d87d1309;
  parent 04b118a4 pins the merged engine. The bounded preview recorder passes
  focused host checks but is not native-linked or installed on COM17.
  Preserve dirty/untracked wizard/map edits, evidence, artifacts and recovery.
- OrcMaps worktree: F:/AI/OrcSDR-TEMP/orcmaps-quality-delivery. PR #6 is merged
  at 722d0d382f99b2887694af333f85b1ee048f4208, now pinned in OrcSDR source/lock.
  This merged pin has focused consumer checks but no native/device acceptance.
- COM17, USB serial 30:ED:A0:E2:E6:95, is our maps device. Last accepted app is
  maps gfx-fix dirty 2026-10-18, using engine 63f0573 and C6 3.0.6. Verify its
  actual state and recovery before any update.
- Claude owns COM39 for the Dead Air dashboard and may flash it. Do not use its
  serial port, reset it or flash it without a coordinated handoff. Preserve the
  user's COM39 C6 1.4.1 requirement. Subagents are allowed for useful bounded
  tasks with explicit file and device ownership.

The required result is a reusable OrcMaps adapter, fluid readable offline maps,
one global regional-detail base plus optional local packs, and a self-contained
OrcSDR-branded setup flow. English names first, then local names. Show appropriate
country/state/county boundaries, places, major roads, water and land cover where
source data supports them. Google/Mesh Map is a quality reference, not a source
of redistributable map content. Keep pack tooling in OrcMaps; no separate
OrcTiles project. PMTiles and legacy ORCMAP1 remain distinct; preserve both data
and explicit compatibility behavior.

Work through these remaining milestones:

1. Reconcile bounded preview timing, validate the merged engine natively, and
   measure continuous cached pan/zoom feedback on COM17 under reception.
   Record p50/p95/max, cancellation, render/transfer/SD costs, cache behavior,
   PSRAM and free/largest internal DMA blocks. Aim for roughly 33 ms cached
   motion; do not claim it from host tests or worker timings containing dwell.
2. Reuse the existing 543,380,195-byte global candidate with verified metadata
   and hashes. Validate real device reference views, zoom-dependent labels,
   antimeridian behavior, boundary hierarchy, international fonts and warning
   presentation. Stop and diagnose unexpected rendering changes.
3. Implement the owned catalog and trusted resumable installation: immutable
   identities, size/free-space checks, TLS/hash verification, staging, atomic
   activation and recovery. Preserve manual imports and prior working packs.
   Model storage/delivery cost for 20,000 users before selecting hosting.
   Get my decision when a hosting/publication choice is actually required.
4. Finish wizard-owned Welcome → Wi-Fi → Location → Maps → Download → Ready.
   Keep password/address/coordinate inputs inside setup. Support network,
   address, map and direct coordinates with explicit location confirmation;
   Back, Skip, Cancel, interruption recovery and offline/no-SD fallbacks.
   Preserve existing settings, readable 1280×720/5-inch text and approved
   orientation behavior. Pause reception throughout and restore prior state.
5. Complete targeted navigation/radio/audio, retained-map SD, low-memory and
   pairing-headroom checks on the final candidate. Cover setup Finish/Cancel,
   hotplug and previously active audio restoration. Test true first boot only
   with verified backups and separate explicit erase authorization.
6. Reconcile/commit the preserved implementation, review the final change,
   obtain explicit OrcSDR merge approval, and verify the merge on remote main.
   Keep releases/tags/map publication as separate approvals.

Protect the working RTL driver, 40 KiB DMA reserve, receiver lifecycle and
framebuffer ownership. No unrelated main.cpp/DSP/USB/Wi-Fi/audio restructuring,
no deferred Wi-Fi-crash investigation, no new overnight soak requirement, and
no generic map-provider or custom-area feature expansion. Never edit managed
component sources, vendor the engine, or invent upstream APIs.

Follow AGENTS.md: narrow scout and tests first; request concrete scoped approval
before full suites, repeated native build/test cycles, large-data work or large
reviews. Batch related approvals when the candidate and test scope are ready.
Past completed approvals do not cover new operations. Preserve NVS, SD, embedded
world, C6 and recovery unless I explicitly approve otherwise. Do useful independent
work while waiting; do not bypass an unanswered required approval.

For each milestone report files/commit, exact tests and actual results, device,
saved log path, remaining blocker and next step. Save logs beside the invoked
test script/program. Keep source, build, flash, physical acceptance, merge and
publication claims separate. Update the plan without erasing historical evidence.
Do not declare completion while a required gate is untested or failing.
