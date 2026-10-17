# Shared setup and location contract

Baseline canonical state is receiver_location::Snapshot (receiver_location.hpp:7): configured, E7 coordinates, label, map_pack, revision. `main.cpp:15000` still loads legacy adsb_loc_set/lat/lon keys before publishing canonical state. Existing Settings/manual and confirmed IP actions must converge on one explicit commit callback.

## Proposed event-driven state machine

Welcome -> optional Network -> Location -> Maps -> Completion, with Back/Skip/Cancel and Settings > System > Run Setup Wizard. No automatic network lookup or download. Network is optional; manual E7 entry always works; crosshair/pan/zoom uses OrcMaps camera with available local/firmware data. A missing basemap offers honest coordinate/grid fallback, not a fake geographic map. An IP estimate is approximate and requires separate acceptance.

A picker edits a proposal. Only Confirm commits receiver location; moving camera, pack selection, cancel/back/skip/rerun must not overwrite canonical location, map_pack or tuned receiver state. Returning from optional network settings resumes the wizard without blocking UI/radio servicing. Keyboard/focus follows existing mechanisms.

## Persistence and upgrade rules

Missing setup_done alone does not make an existing user new. Classify a blank install using existing configuration/schema presence, not only location: an existing user can intentionally have no location. Unknown future setup versions are not overwritten automatically. Completed existing installs remain untouched; interrupted current-version setup resumes at its recorded step, preserving confirmed location. Rerun starts a session without clearing the existing completion/location record; cancel restores return surface and does not erase anything.

Historical setup_wizard/model/store files were inspected on claude/orcmaps-integration. Salvage pure state/model tests only after correcting its store: missing keys force setup, Save writes adsb_loc_set even for skipped/rerun sessions, and the prototype run_first_run_setup skips welcome/network and blocks inside picker. None of those behaviors is acceptable to import.

Boot integration must follow M5Unified/Hosted/SD staging order, use a registered screen_controller surface, and keep radio_session ownership unchanged. main.cpp may contain bounded init/tick/action/commit calls only. Tests must cover blank/legacy/interrupted/future records; confirm/cancel/back/skip/rerun; valid E7 boundaries; map-pack preservation and receiver-state preservation.

Status: design documented; production implementation/acceptance not yet complete.
