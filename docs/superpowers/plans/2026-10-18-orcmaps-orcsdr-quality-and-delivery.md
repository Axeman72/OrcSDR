# OrcMaps / OrcSDR Map Quality and Delivery — Approved Execution Plan

> **For agentic workers:** Execute with superpowers:executing-plans. The user approved execution on October 18, 2026 ("approved. please enable this."). Current work is Phase 2 map quality and Phase 3 Tab5 responsiveness; outstanding Phase 1 gates remain open. This approval does not authorize paid services, publication, destructive operations, or bypass the user's approval requirement for high-usage work.

## Current phase and handoff — October 18, 2026

**Active: Phase 2 and Phase 3. Neither phase is complete.** Labels are implemented
and running on COM17; the immediate priority is measured responsiveness and
memory headroom. Boundary hierarchy and worldwide font/quality acceptance are
still required. The execution ledger below is chronological history; earlier
statements such as "not pushed" or "approval pending" describe that earlier step,
not the current state.

| Phase | Current status |
| --- | --- |
| 1 — baseline | Candidate, hashes, reference views and bounded P4 measurements recorded. Full source/view acceptance and final budgets remain open. |
| 2 — labels/style | Bounded labels and real-font adapters implemented; host and COM17 evidence recorded. Boundary hierarchy, contrast and international coverage remain open. |
| 3 — fluidity | Cached preview tested on COM17 and physically accepted: moves with the finger and feels noticeably better. Bounded RF, SD and memory-fallback checks passed. Continuous-motion timing and the 33 ms target remain unverified. |
| 4 — catalog/delivery | Candidate size/provenance and preliminary delivery model recorded. Trusted catalog, resumable installation and publication remain incomplete. |
| 5 — setup | Wizard-owned UI is prepared and included in the tested dirty build. Complete flow, downloads, interruptions and physical acceptance are incomplete. |
| 6 — integration | OrcMaps PR #6 merged and verified on origin/main at 722d0d3. OrcSDR source pin/lock updated with focused checks; no OrcSDR main merge, release or device update of that pin yet. |

### Latest verified device state

- COM17: latest approved app built from `477f087a` plus the repeated-Settings
  graphics correction, setup reception pause and the city-label zoom policy
  and preserved wizard/gesture/blackout edits; new timing instrumentation excluded;
  app-only flash and boot passed. Follow-up evidence commit `477f087a` changes
  documentation only. OrcMaps pin: `63f0573e98351ef50eb3debeca5ee07a0a9f2eeb` on its approved
  development branch. Driver 0.9.3; C6 unchanged at 3.0.6. COM39 untouched.
- Receiver Location overflow corrected: long map identity is bounded with an
  ellipsis; full stored identity retained. Host regression and captured COM17
  return from Use Location pass. Physical-panel confirmation remains separate.
- Full native build passes pin/miniz/FT8 gates. Label pool occupies 15,416 bytes
  in PSRAM; configured internal DMA reserve remains 40,960 bytes. App size
  5,060,384 bytes, approximately 20% app-partition space remaining.
- Bounded navigation/map reception checks reported zero driver drops/overruns
  around 2.4 MSPS. The latest two-frame holder retained 2,835,456 bytes during
  unique 8 KiB SD scratch upload/readback with matching hashes. Low-memory
  testing also passed with only the 1,417,728-byte worker frame, followed by
  successful reclaim and recreation of both frames. Bulk transfer uses paused reception;
  radio_busy during active reception is expected. No overnight soak required.
- Measured uncached pan/zoom frames: 1.445–1.806 seconds; full-frame pushes:
  76.5–78.4 ms. These are earlier baseline timings, not current continuous-drag
  measurements. **33 ms cached motion remains unverified.** Earlier baseline minimum DMA
  free fell to 3,663 bytes; the latest label-policy run recorded 7,547 bytes minimum
  DMA free. Different workloads prevent treating that as a measured improvement.
  Memory headroom stays an open gate. These are bounded
  observations, not proof of long-term stability or decoded-signal quality.
- Captured labels are readable, but some names are unsupported by the selected
  font. Diagnostics count them; picker presentation of that warning is unfinished.

Evidence: [accepted cached preview and COM17 checks](../../evidence/maps/2026-10-18-preview-com17.md),
[COM17 label tests](../../evidence/maps/2026-10-18-label-com17-validation.md),
[native build](../../evidence/maps/2026-10-18-label-native-build.md), and
[Use Location correction](../../evidence/maps/2026-10-18-location-text-com17.md).
Exact commands, logs, hashes and recovery paths are recorded there.

### Next work

Global discovery gate: the previous device engine pin rejected `protomaps-v4`
at manifest validation before archive access. This was not an SD failure.
The explicit compatibility correction is now merged upstream and the consumer
source pin is updated; it is not installed on either device yet. Earlier evidence:
[global profile discovery gate](../../evidence/maps/2026-10-18-global-profile-discovery-gate.md).
User approved the narrow source/host change. OrcMaps now explicitly accepts
`protomaps-v4`; failing-first validation/discovery regressions pass (1/1 focused
CTest executable). Unknown profiles and existing manifest/archive metadata
guards remain enforced. Code checkpoint `dbb1b98` is in
`F:/AI/OrcSDR-TEMP/orcmaps-quality-delivery`; evidence:
`docs/evidence/2026-10-18-protomaps-pack-compatibility.md` in that worktree.
Focused checkpoint CTest passed again (1/1), log
`pack-profile-checkpoint-20261018.log` in the engine worktree root.
Separately approved full configured host CTest run passed 7/7 at `dbb1b98`;
logs `pack-profile-full-build-20261018.log` and `pack-profile-full-tests-20261018.log`
in the engine worktree. Documentation checkpoint `c39f827` records the run.
Python builder/provenance and separately configured font backends were not part
of this CTest run. User-approved whole-branch review found no actionable merge
blockers. All premerge CI gates passed after correcting stale documentation
counts. OrcMaps PR #6 merged at `722d0d382f99b2887694af333f85b1ee048f4208`;
fetched origin/main contains that commit and its tree matches reviewed head
15c1acf. OrcSDR manifest/lock now pin that merge; unrelated dependencies unchanged
and requested/locked/fetched check passes. Focused consumer map/preview/timing
checks pass 3/3 at c39f827, whose only difference from merged code is STATUS.md.
Real global installation and hardware acceptance remain open. Current COM17
firmware/pin is unchanged by this source work. Merge and consumer evidence:
[OrcMaps main merge](../../evidence/maps/2026-10-18-orcmaps-main-merge.md).
A bounded 127-byte header check
confirms the actual candidate bounds fit validation. Focused tests now match
its bounds/size/zoom and pass zoom-8 selection and zoom-9 refusal; no tile scan
or archive rehash in that follow-up.

Regression found after setup testing: COM17 Home had audio/streaming but blank
plots because runtime graphics was disabled. Serial re-enable restored it and
user confirmed recovery. Repeated Settings opens can overwrite the original
graphics restore value with the temporary false value. User-approved one-line
session-preservation correction now passes focused host regression with the
real screen controller and P4 syntax. Separately approved native build, COM17
app-only flash/preservation and all 14 graphics-status snapshots pass. Initially
ON/OFF states survive repeated Settings, Close/Home and wizard Finish/Cancel.
User physically confirmed **"Both drawing normally"**. This specific regression
is closed on COM17; local source/test/evidence checkpoint `24fcfea5` records the
fix (not pushed). Focused host checkpoint passes; device still runs the same
verified dirty build, not a newly flashed image of that commit. Other wizard
edits remain preserved and uncommitted. See
[Home graphics diagnosis](../../evidence/maps/2026-10-18-home-graphics-disabled.md).
No USB-dropout cause is established by this incident; do not count it as such.

Setup reception policy: user approved pausing dongle reception/audio for the
entire wizard, preserving USB/settings, then restoring only prior reception on
Finish/Cancel. Source hooks, shared I/O ownership and restart guards are prepared.
Focused lifecycle and actual-function ownership tests pass; P4 syntax passes.
Approved native build, inspection, COM17 app-only flash and preservation passed.
Twenty-one driver snapshots verify pause across all wizard steps, a completed
Wi-Fi scan, restoration after Finish/Cancel/external Home, and no restart when
previously stopped. Frequency retained, zero driver drops/overruns. COM17 is
was left on Setup Welcome for that physical check; after the graphics regression
cycle it is back on Home, graphics enabled and receiving.
User physically confirmed **"Notice readable; responsive; no replug performed"**.
Notice and responsiveness are accepted. Physical hotplug, active audio and true
automatic first boot remain unverified on this candidate. COM39/C6/SD/world remain preserved.
Evidence: [setup reception pause](../../evidence/maps/2026-10-18-setup-reception-pause.md).
Keep this gate separate from active-radio map responsiveness acceptance outside
the wizard; pausing setup does not close that performance requirement.

Latest requested refinement: hide city/town names on the default world view.
The shared OrcSDR pipeline now excludes place-label layers below zoom 6;
closer views retain the existing per-feature visibility and collision rules.
Local source/test/evidence checkpoint `c0af86fb` records this change (not pushed).
Focused real-archive checkpoint test passed 1/1, log `city-label-checkpoint-20261018.log`.
It is installed on COM17 only. User physically
accepted the latest candidate: **"World view clear; controls and dragging good"**.
Focused host build and `ctest --test-dir .local/maps/preview-pinned -R '^map_pipeline$'
--output-on-failure` passed 1/1 with the real global archive: zooms 3 and 5
offer/draw no place names, while zoom-8 Eugene/London checks still pass.
Follow-up transition checks also pass: names draw at zoom 6 and disappear on
return to zoom 3. Logs: `city-label-transition-build-20261018.log` and
`city-label-transition-test-20261018.log`; focused result 1/1 passed.
Evidence: [place-label zoom policy](../../evidence/maps/2026-10-18-place-label-zoom.md).
Logs: `city-label-build-20261018.log` and `city-label-test-20261018.log` in the
integration worktree root. The first native artifact was rejected before flash
for missing explicit cached inputs. The corrected cycle was separately approved
and executed: explicit inputs, native
inspection, app-only write and preservation checks pass. Seven overview status
captures offer/draw zero names; bounded loaded pan/zoom passes, driver drops and
overruns zero, C6 unchanged at 3.0.6. Physical overview/control/drag check passed.
Zoom-6 name appearance remains host-verified only. Latest DMA free 10,395,
minimum 7,547 bytes; memory headroom remains open. Full commands, hashes and
logs are recorded in the place-label evidence. COM39 remains untouched.

Cached-motion checkpoint: published OrcMaps transform 63f0573 is pinned through a
regenerated lock. The prior COM17 candidate d30b5765 plus preserved wizard/gesture/blackout
edits introduced the immutable PSRAM-preview adapter. User physically confirmed
**"Moves with finger; noticeably better"**. Native build/placement, six loaded
pan/zoom actions, low-memory snapshot fallback, release/recreate and retained
two-buffer SD scratch read/write checks pass. Final RF drops/overruns zero.
Evidence: `docs/evidence/maps/2026-10-18-preview-com17.md`. Continuous-motion
latency/33 ms acceptance remains open; do not equate physical improvement with
meeting that timing target. This supersedes earlier single-canvas/pending-push
statements in the historical ledger and summary.

Physical acceptance update: COM17 drag/tap behavior was confirmed by the user.
The reported blackout during map replacement was reproduced, corrected and
flashed in the approved build/test cycle. User confirmed **"Map stays visible"**.
Evidence: `docs/evidence/maps/2026-10-18-picker-blackout.md`. This closes the
blackout defect, not Phase 3 fluidity: uncached frames still take 1.29–1.74 s.

1. Opening-view label refinement is physically accepted on COM17, with controls
   and cached dragging preserved. Names at closer zoom are
   host-verified and need suitable installed data for device acceptance.
2. Drag-start correction is now hardware accepted. Focused red/green
   host checks now prove touch-down does not recenter, dragging does not jump,
   taps still recenter on release, small jitter is tolerated and outside release
   cancels a tap. Source changes remain alongside preserved wizard edits and
   were included in the COM17 candidate. Evidence: `docs/evidence/maps/2026-10-18-picker-gesture.md`
   and `docs/evidence/maps/2026-10-18-gesture-com17.md`.
3. Profile and improve cached motion/display transfer and worker rendering,
   preserving one camera/generation for geography, labels and overlays. Measure
   p50/p95/max feedback intervals and cancellation, not just average frame cost.
   Source scout confirms the current recorder excludes cached-preview paints;
   it cannot establish the motion gate. See
   [preview timing measurement gap](../../evidence/maps/2026-10-18-preview-timing-scout.md)
   for measurement boundaries and nonblocking diagnostic requirements.
4. Recheck internal/DMA headroom, retained-map SD and active-radio continuity.
5. Finish boundary hierarchy/contrast and worldwide glyph/view acceptance, then
   complete trusted delivery and the self-contained wizard. All original phase
   gates remain in force; the above is sequencing, not a reduced objective.

## Execution ledger

- October 18 latest checkpoint: cached movement physically accepted on COM17
  ("Moves with finger; noticeably better"); evidence recorded at `477f087a`.
  User then requested city names only at closer zoom. Shared pipeline policy
  and focused host regression are prepared and pass; no subsequent native build
  or flash has occurred. Phase 2 and Phase 3 remain open.
- October 18: execution approved; existing wizard changes preserved. Read-only local archive inventory and three single-tile inspections completed. No firmware build, flash, download, or device operation in this assessment.
- Evidence: [Worldwide-base assessment](../../evidence/maps/2026-10-18-worldwide-base-assessment.md). Phase 1 remains open: no complete global regional-detail candidate or hardware performance baseline yet.
- Ruling: keep the embedded Natural Earth overview as the fallback, not the regional base. Its declared layers omit roads and land cover; its builder includes country/state boundaries but no county source. This avoids promising unavailable detail.
- Ruling: do not relabel a different vector schema as OpenMapTiles. A candidate with different layer/property conventions requires a verified adapter or a compatible production profile.
- October 18 continuation: reviewed the shared 0.3.0 cache diff and CYD evidence limits; reproduced and corrected thick-stroke cache bounds on isolated OrcMaps branch `codex/map-quality-delivery` (focused red then green test). COM17 read-only baseline captured with reception active, C6 unchanged; fresh gesture measurements remain open. Public global-build metadata returned HTTP 403; no bulk download attempted.
- OrcMaps fix committed locally as `10fc262`; four scoped files, focused `orcmap_screen_bounds_tests` 1/1 pass after eight thick-stroke failures before the fix. Not pushed, pinned, merged or flashed. Root red/green logs retained in the OrcMaps worktree.
- October 18: user approved one global candidate extraction capped at 600 MB. Acquired z0–8 candidate, 543,380,195 bytes, SHA-256 `16adde0b4f67734535db6871b89949e68c7e1dabf1c34d2bd7bd23842a9e5991`; proxy enforced ETag/ranges and transferred 543,439,268 bytes in five requests. Structure verification and eleven bounded reference tile decodes pass. Protomaps-v4 classification remains unsupported; this is source-data evidence, not rendering acceptance. Evidence: `F:/AI/OrcSDR-TEMP/orcmaps-quality-delivery/docs/evidence/2026-10-18-global-regional-candidate.md`.
- User selected English place names where available, then local names. Retain UTF-8 data and verify glyph support; this is not authorization to silently drop unsupported local names.
- Global candidate/acceptance evidence committed in OrcMaps as `70dcb47`. Explicit Protomaps-v4 engine classifier committed as `5572ed6` passes focused red/green tests, preserving existing profile behavior and source names/hierarchy fields. Actual 1280x720 Eugene host render finds 24/24 tiles; visible tile-edge grid and missing labels mean quality is not accepted. Next: reproduce/fix multi-tile seams, then labels/hierarchy styling. P4 pin and both devices remain unchanged by this work.
- Ruling: add an explicit engine-owned profile for the measured candidate instead of changing its metadata to OpenMapTiles. Profile classification does not by itself close rendering, licensing, global quality or hardware gates. Remaining Phase 1 hardware timing/glyph details stay open while bounded host correctness work proceeds.
- October 18 continuation: tile-edge grid reproduced in six synthetic cases across immediate/streamed/cached drawing and both tile orders. Prepared native-scissor fix in OrcMaps (uncommitted): composition/stroke bounds 2/2 focused tests pass; parent/empty/offscreen clip checks pass. Actual Eugene 24-tile host render no longer shows the grid. Full host regressions and adapter checks requested for approval; no native build, pin change or flash. Evidence: OrcMaps `docs/evidence/2026-10-18-tile-composition.md`.
- User approved the full host/adapter checks. Full build and 4/4 CTest entries pass; real SDK syntax checks pass for P4 map consumer, P4 renderer and CYD consumer. Seam fix committed locally as OrcMaps `5437b8c`. No native link/flash or P4 performance claim. Concrete bounded label API/resource design prepared in OrcMaps `docs/PLACE_LABEL_DESIGN.md`; original plan left final text API choices to the baseline review.
- Provenance continuation: added a distinct prebuilt Protomaps-v4 source record with publisher terms, input identity and explicit separation from raw OSM/source-policy versus pack/release approval. Candidate metadata reports v4.14.3 and OSM replication time March 31, 2026. Targeted provenance validator passes (11 records, 0 errors/warnings), log `global-candidate-provenance-20261018.log`. Found two still-closed compatibility gates: engine pack validation and OrcSDR streaming pipeline; do not enable them based solely on the host batch classifier. Label-design approval remains pending; no devices changed.

**Goal:** Deliver useful, fluid, offline-capable maps and a self-contained OrcSDR setup experience, with a catalog and reproducible map production under our control.

**Architecture:** OrcMaps owns reusable rendering, labels, camera behavior, caches and pack compatibility. OrcSDR owns setup, catalog browsing, verified installation and radio overlays through the shared map adapter. Map production, catalog schema/publishing and reusable pack verification live in OrcMaps; there is no separate OrcTiles product/repository. Published catalogs and storage remain replaceable.

**Tech stack:** Existing native ESP-IDF/M5GFX Tab5 application, OrcMaps C++, existing host pack builders and PMTiles-compatible vector data. Hosting and catalog transport remain decisions, not dependencies selected by this document.

**Spec:** User's Google Normal / Mesh Map screenshots and approved six-screen setup mockup. Orc branding and palette; state/county boundaries, city/town names, roads, water and forests; fluid interaction; self-contained Wi-Fi/location/download setup. The screenshots are visual references, not permission to redistribute Google map content.

## Confirmed user decisions — October 18, 2026

### Latest execution evidence — October 18 continuation

- User approved the bounded label design. OrcMaps `5f3a1eb` implements the
  fixed 128-candidate/64-placement layout, real-font adapters and profile-aware
  streaming classification. Full host regression before the final classifier
  extraction: 5/5 CTest programs passed; focused classifier test after extraction:
  1/1 passed. Real-font test: 1/1 passed. Eight worldwide z8 previews inspected;
  these are host evidence, not final style, script-coverage or device acceptance.
- User approved the development-branch push. Remote OrcMaps
  `codex/map-quality-delivery` was verified at full SHA
  `5f3a1eb767b26aa7df00158308f40331b298d7ef`. No main merge or release occurred.
- OrcSDR `66afa335`/`8d1eaa48` pin that engine through a component-manager
  regenerated lock; unrelated dependencies compare unchanged. Shared streaming
  integration collects labels in caller-owned storage; map service places the
  fixed layout in external RAM and reports unsupported-name counts.
- Focused OrcSDR host test with the actual global candidate passes, including
  repeat rendering and Eugene-to-London label reset. Two P4 syntax checks pass.
  Evidence: `docs/evidence/maps/2026-10-18-streamed-place-labels.md`.
- User approved the native build and, after an environment-related failure,
  one corrected retry. Corrected native build passed: 5,051,280-byte app; labels
  occupy 15,416 bytes in .ext_ram.bss; DMA reserve stays 40,960 bytes; pin, miniz
  and FT8 placement gates pass. Evidence: `docs/evidence/maps/2026-10-18-label-native-build.md`.
  No devices were changed; hardware approval and acceptance remain pending.
  Existing wizard edits remain preserved and uncommitted. The global profile
  remains closed at pack validation/discovery, pending full compatibility work.
- Boundary scout: country/region/county source information currently collapses
  to generic kBoundary, drawn similarly to roads. Proposed narrow engine scope
  is recorded in OrcMaps `docs/BOUNDARY_HIERARCHY_DESIGN.md`; not implemented.
  All phase gates below remain governed by their actual acceptance evidence.

1. Offline maps are the first-release baseline after download. Internet is for acquisition and updates, not a requirement for installed maps.
2. The first release targets worldwide regional detail, not an Oregon-only or U.S.-only release.
3. A global regional-detail base is the default downloadable map, with optional higher-detail local packs.
4. Map-building/catalog tooling is part of OrcMaps. “OrcTiles” was not a request for a separate repository/product.
5. Quality reference: Google Normal in the Meshtastic / Mesh Map Android app, including the supplied Oregon regional screenshot. Use our own licensed data and styling.

The built-in overview remains the no-SD/no-network fallback. It must not be described as equivalent to the global regional-detail base. Regional means useful cities/towns, administrative boundaries where supported by the data, major roads, water and land cover; worldwide street/building detail is not part of the default pack.

Standard compatible PMTiles packs, our own signed catalog, replaceable hosting and manual compatible-pack import form the initial design. Optional custom online providers and custom-area build queues are later work. No cloud account, subscription, publication or spending without approval.

### Decisions settled by measurement before production

- Exact base-pack bytes, zoom cutoffs, simplification and feature inclusion; no small-size promise before a real build.
- One logical global base versus transparently managed physical shards. The UI should present one global product; sharding must not silently change it into user-selected regions.
- Worldwide glyph coverage. User selected English names where available, then local names; never silently drop unsupported place names or claim native-script parity without testing.
- Hosting provider, retention/update cadence and spending limit after pack-size and 20,000-user cost estimates.
- Numeric hardware pass budgets after baseline measurements; the initial cached-motion target remains approximately 33 ms.

## Initial planning baseline — historical, superseded by current handoff above

- OrcSDR checkout: F:/AI/OrcSDR-TEMP/maps-integration-completion, branch codex/maps-integration-completion, last commit dfde5cfd. Work-in-progress wizard-owned screens are preserved, uncommitted. Their four focused host tests passed before implementation was paused; native build and hardware acceptance remain untested.
- Cached origin/main comparison: 0 behind, 37 ahead. Refresh/reconcile before final PR preparation; this is not a claim that remote main was checked live during this planning pass.
- OrcSDR pins OrcMaps dc6bc12 (v0.2.3). Local F:/Ai/OrcMaps/main is clean at 86e27a0 with version 0.3.0 and new caching/rendering work. Publication/tag status not established by the local version number.
- Current renderer explicitly skips label features. Existing packs or a provider URL alone cannot supply city labels on Tab5.
- Existing host builders already produce world-overview and regional PMTiles/manifest/checksum triplets. Reuse and validate them before creating another builder.
- OrcMaps world-overview-1 release was read-only verified as a draft. Its overview archives are not a published regional catalog or a worldwide street-detail map.
- OrcSDR's signed historical data catalog is distinct from arbitrary OrcMaps regional discovery; legacy ORCMAP1 is distinct from PMTiles.

## Global constraints

- Protect main/master in both repositories; preserve all current work, logs and recovery images. Use scoped branches/worktrees and review existing integration work before overlapping changes.
- New OrcSDR worktrees, if needed, belong under F:/AI/OrcSDR-TEMP/. No automatic rebase, force push, merge, release or publication.
- COM17 is the initial P4 acceptance device; retain C6 3.0.6, NVS and SD data. COM39 retains C6 1.4.1 and is untouched unless a later comparison test is explicitly selected.
- Preserve RTL driver 0.9.3, full 40 KiB DMA reserve, existing display ownership and receiver servicing. Avoid unrelated main.cpp/DSP/USB/audio/Wi-Fi restructuring.
- Embedded overview must work without SD or Wi-Fi. Regional download failure must not remove working maps.
- Setup uses its own pages and inputs, while sharing backend services. No jump into Settings for Wi-Fi or location.
- Target Tab5 typography: 40 px headings, 32 px main text, 24 px secondary text; main touch targets approximately 80 px high. Verify on the physical 1280x720, 5-inch panel.
- Respect source licenses and attribution; preserve source dates, hashes, derivation and compatibility metadata.

## Review focus

- Label density and county geometry vary with zoom; a tile file may omit required features even when its declared zoom range is sufficient.
- Cached motion can hide stale pixels or wrong projections. Basemap, labels and overlays must share one camera and generation.
- Map memory must remain bounded in PSRAM and must not recreate the DMA shortages affecting SD and pairing.
- Downloads must handle interrupted power/network, corrupt files, missing/removed/full SD cards and server changes without activating partial content.
- Interrupted setup, cancellation and migration must retain existing location, map choice, credentials and unrelated settings.

## Phase 1 — Establish a reproducible quality and performance baseline

Files: focused evidence notes in both repositories; existing pack-builder manifests/profiles and benchmark tooling. No renderer redesign yet.

- [x] Review the exact 0.2.3 -> 86e27a0 engine changes, APIs, tests and hardware evidence; distinguish reusable engine cache APIs from CYD-demo-specific logic. Shared cache reviewed; bounds regression corrected separately; CYD evidence is synthetic and not Tab5 proof.
- [x] Inventory the existing OrcMaps world and regional builders. Keep all pack-building and catalog tooling in OrcMaps; do not create an OrcTiles repository. Inventory recorded in the worldwide-base assessment and OrcMaps builder documentation.
- [ ] Verify available worldwide source datasets and existing Oregon/Springfield fixtures for place names, administrative boundaries, roads, rivers and land cover. Save exact source and pack hashes; do not broadly inspect large fixtures.
- [x] Build/inspect a representative global regional-detail candidate before setting a download budget. Measure which zooms retain county-equivalent boundaries and towns: an overview archive with larger zoom metadata is not sufficient. Actual z0–8 candidate and z7/9 detail samples inspected; county kinds present in eight regional z8 samples. This does not establish production schema compatibility or complete worldwide quality.
- [x] Define a small worldwide acceptance set spanning continents, dense/sparse areas, an island/antimeridian case and international scripts; include Oregon regional, Lane County and Springfield/local as repeatable reference views, not release-coverage limits. Record expected names and boundaries at each. OrcMaps `tools/pack-builder/global-base-acceptance.json` defines eight views, expected place alternatives, boundary context, zooms and acceptance limits.
- [x] Capture current Tab5 map timings/memory and active-radio counters using bounded tests. COM17 baseline recorded above; gesture percentiles remain a Phase 3 requirement. No overnight soak requirement.
- [ ] Set final gesture/render/label budgets from measured baseline and the user's quality expectations.

**Gate:** Reproducible source/pack/view inputs, documented gaps and measurements. No assumption that a later engine version or higher zoom pack alone solves quality.

## Phase 2 — Add labels and geographic styling in OrcMaps

Primary files: src/render/renderer.cpp, include/orcmap/render_target.hpp, style and feature/classification interfaces; focused new label layout implementation/tests; existing host/M5GFX adapters. Final API shape follows Phase 1 review.

- [ ] Write failing tests for place-name extraction, priority, viewport clipping, bounded label count, overlap rejection, stable selection, UTF-8 safety and supported-script/fallback behavior. Treat international names as a first-release global-base requirement.
- [x] Add the minimum graphics-independent text/measurement capability and label placement needed for settlements and selected regional labels. OrcMaps 5f3a1eb; broader script/quality acceptance remains open.
- [ ] Retain and distinguish country/state/county boundary information; style states and counties visibly at appropriate zooms.
- [ ] Preserve useful road hierarchy, water, forests/parks and quiet colors beneath bright radio overlays.
- [x] Use bounded PSRAM-backed candidates/scratch on P4; avoid per-label unbounded allocation. Fixed pool linked in external BSS; tile RenderScratch reused. Runtime memory headroom remains open. No full MapLibre style interpreter.
- [ ] Add real-geography comparisons at the fixed views, plus malformed/oversized-name and dense-city tests. Maintain miniz and polygon-ring gates.
- [ ] Verify CYD/other existing adapters remain functional where interfaces change; do not claim Tab5 performance from desktop/CYD tests.

**Gate:** Readable, deterministic labels and required boundaries in reference renders; compatibility checks pass; bounded allocation failure is safe.

## Phase 3 — Prove fluid interaction on the Tab5

Primary files: reviewed OrcMaps camera/cache APIs and OrcSDR ui/map_pipeline.cpp, ui/map_service.cpp and map picker integration.

- [x] Integrate one verified engine revision through the component pin and regenerated lock. OrcMaps 5f3a1eb; unrelated dependencies unchanged, native pin gate passed, COM17 boot/render tested. Never edit managed_components sources.
- [ ] Reuse engine cache work where appropriate. Keep a bounded map cache and reuse rendered geography during gestures instead of synchronously decoding on every touch.
- [ ] Cancel superseded requests; prioritize the newest viewport and keep final labels/overlays aligned with it. Preserve single UI ownership of framebuffer pushes.
- [ ] Measure touch-to-feedback and p50/p95/max visible-update intervals, cache hit/miss behavior, render/SD/transfer time, PSRAM, free/largest internal DMA block and cancellation response.
- [ ] Target roughly 33 ms visible motion updates on cached pan/zoom; treat this as a hardware target, not a claim that every uncached tile can render in 33 ms. Agree numerical pass gates after Phase 1.
- [ ] Repeat tests while receiving, monitoring IQ/audio loss, USB errors and resets. Exercise HOME <-> RF Lab <-> maps/Settings transitions and confirm no stale screen content.
- [ ] Keep retained-map SD read/write regression checks and pairing memory headroom as explicit gates.

**Gate:** User accepts physical map quality and interaction; measured budgets and radio/SD continuity pass on COM17. Otherwise optimize the measured bottleneck before expanding delivery scope.

## Phase 4 — Own the pack catalog and delivery pipeline

Primary files: existing OrcMaps pack builders/verification, a small catalog schema/publisher, and a shared OrcSDR map acquisition adapter with focused storage/download tests.

- [ ] Define pack entries: stable ID/name, bounds/coverage, detail/zoom, schema compatibility, source/build date, download bytes, SHA-256, attribution/license, immutable URL and mirrors.
- [ ] Sign trusted catalog metadata using a reviewed approach consistent with existing project trust practices, without confusing the historical data catalog and map manifests.
- [ ] Build one logical global regional-detail base and optional local-detail packs. Reuse identical versioned artifacts across users, validate them, and publish only after approval. Recommend local packs by location without generating a unique pack for every user by default.
- [ ] Measure representative pack sizes/build cost. Model storage, reads and delivery for 20,000 users before choosing hosting; keep catalog URLs/provider replaceable.
- [ ] Implement asynchronous resumable install with explicit size/free-space checks, bounded buffers, verified TLS/hash, staging and atomic activation preserving previous packs. Resume must validate object identity, not append unrelated versions.
- [ ] Test short/corrupt/wrong-schema downloads, cancellation, network/power interruption, full/removed SD, version replacement and hash mismatch.
- [ ] Keep manual import of compatible packs available without our hosting service. Preserve explicit legacy support/migration.

**Gate:** The global regional-detail base passes the worldwide acceptance set and can be downloaded, verified and used offline alongside optional local packs. Actual pack size and 20,000-user delivery costs are recorded. Failed/interrupted downloads preserve working data; hosting/publication approvals are complete.

## Phase 5 — Finish the self-contained branded setup

Files: preserved ui/setup_ui.hpp/.cpp work, map_screen, small backend hooks, focused host tests and setup persistence.

- [ ] Reconcile the prepared UI work with the approved branded preview and the real acquisition API; preserve its passing tests.
- [ ] Complete Welcome -> Wi-Fi -> Location -> Maps -> Download -> Ready, with wizard-owned password/address/coordinate inputs, Back, Skip, cancellation and interrupted recovery.
- [ ] Present network location as approximate; confirm each proposed location before saving. Show the saved point clearly.
- [ ] Offer the global regional-detail base as the standard download, then recommend optional local packs after location selection. Clearly distinguish built-in overview, global regional base and local detail; show size/status/progress/error/retry and no-SD/offline states.
- [ ] Reuse the actual Orc badge/palette. Test font/layout/targets at native resolution and physical size; keep the selected fresh-install orientation.
- [ ] Test true first boot only with fresh verified backups and explicit erase authorization; test rerun without erasure as well.

**Gate:** A user can complete all setup methods without leaving the wizard; saved location, chosen maps and interrupted progress survive reboot; offline/no-SD paths remain usable.

## Phase 6 — Expand optional local coverage and integrate reviewed work

- [ ] Expand optional higher-detail local packs through the proven build/catalog process. Worldwide regional-base acceptance and international-name handling must already pass before first-release claims.
- [ ] Consider custom-area build queues and selected online provider adapters only after the offline baseline is proven. No generic “any map API” promise.
- [ ] Run the narrow relevant regression matrix, expanding shared-engine checks where shared infrastructure changed.
- [ ] Prepare separately reviewable OrcMaps and OrcSDR changes, record exact pins and dependency order, obtain the required reviews and explicit merge approval.
- [ ] Verify actual merge commits on remote main before claiming landed status.

## Reporting and stop points

Each phase reports files/commit, exact tests/results, device, saved log paths, remaining blockers and next milestone. Save hardware logs beside the invoked test program/script. Stop for missing required input, paid deployment/publication, destructive operation or a new expensive build/test cycle outside the user's approved scope. Never mark an untested phase complete.
