# Dead Air — native Tab5 development handoff

Status: implementation milestone 1; **not** a hardware-tested release.
Work branch: chatgpt/dead-air-native-ui-20261018.
Target hardware: M5Stack Tab5 (ESP32-P4, 1280 x 720, 5-inch touchscreen).

## Design contract

- The approved design is Proposal A: science-first, grounded in observations. Avoid ghost/spirit/entity detector claims. No fabricated numerical data, spectra, waterfals, recordings or probability scores.
- Prefer a 5-inch-usable hierarchy: large content, readable default fonts, a 52 px minimum tab/action height. Six tabs: Overview / Spectrum / Audio / Environment / Events / Review.
- Native ESP-IDF + M5GFX / M5Unified, using the existing shared dashboard header. Avoid HTML and avoid drawing from the DSP/USB callback.
- ENV III (SHT30 + QMP6988 temperature, RH and pressure) is optional. Not present, warming-up, stale or invalid readings show "--" and cannot produce valid detector events.
- Display RF power as **dBFS** until a rigorously supported calibrated dBm path exists. Never call a tuned frequency a simultaneously monitored sweep window.

## Added files and behavior

- apps/orcsdr-tab5/ui/dead_air_model.hpp and .cpp: observation data model, 48-event bounded ring, three RF anomaly sensitivity presets (+12 / +8 / +5 dB relative to valid frequency-local baseline), 12 valid sample warm-up, 200 ms minimum persistence, 3 s cooldown, retune/config revision reset, manual event markers, source names, and no stale readings represented as live.
- apps/orcsdr-tab5/ui/dead_air_dashboard.hpp and .cpp: six M5GFX-native panels, shared OrcSDR header and bottom tabs, normal receiver and sensor unavailable states, plots only when snapshot validity flags are asserted, Event Model threshold buttons and manual event marker action.
- tests/dead_air_model_tests.cpp: host tests for reading freshness, sensitivity, event debounce, retune discontinuity, bounded ring and manual markers.
- Existing dashboard registry / screen controller / native component CMake configuration updated to include Dead Air screen identity and compile the new sources.

## Known integration blocker

The GitHub connector returned an empty main.cpp file, including small line-range reads, because the source is extremely large. Avoiding unsafe blind edits, this branch **does not modify apps/orcsdr-tab5/ui/main.cpp**. The descriptor appears in the dashboard registry, but the screen will NOT open through Home until a local adapter handles the application-level transition and touch/receiver events.

Not yet implemented or tested:
1. Live receiver/IQ/audio snapshot source or tuner-settled signals; the screen intentionally renders no synthetic waterfall.
2. Real audio recording, replay, clip collection, hashes, or exports.
3. Durable append-only SD event/session journal. Automatic RF event detection currently records to bounded RAM only.
4. ENV III I2C probe, external power control, data freshness, per-sensor detector policies.
5. Build with the pinned ESP-IDF/managed M5GFX dependency and physical Tab5 checks.

## Next local integration, staged

1. Review the branch against the actively changing main worktree and reconcile registry/screen-controller edits before merge.
2. Inspect ui/main.cpp and the current Home action dispatch. Add an application-level Dead Air owner (model, snapshot adapter and lifecycle). Under ScreenController: call begin_transition(screens::Id::dead_air), then dead_air::attach_events(&owner.model), dead_air::enter(owner.snapshot), then finish_transition(). Return via the existing Home navigation path.
3. On the existing UI loop, and only when screens::owns(screens::Id::dead_air), call dead_air::update(snapshot) when the snapshot revision changes. Route dead_air::handle_touch results to the real app action dispatcher.
4. Construct real Snapshot values from existing RTL-SDR and audio APIs. Only set spectrum_valid/audio_envelope_valid when actual samples are supplied. Set monotonic time and receiver configuration revisions on RF observations, and observe_rf from the UI task. All retune and settle gaps must invalidate detection.
5. Add a bounded event queue and separate low-priority SD writer; append NDJSON with event sequence, timestamps with clock quality, frequency, units, threshold/baseline and raw media paths. Do not access SD inside IQ/DSP callbacks; account for storage failures and queue overflows.
6. When ENV III hardware arrives: verify I2C wiring, external power/voltage and device presence, sample all three channels, propagate present/valid/age to the Snapshot. Keep normal RF operations and Event Log fully functional when module is absent.
7. Add proper clip capture and replay with original audio and optional IQ, pre/post-trigger context, event-linked notes, and processing provenance.

## Host test instructions

From repository root on Linux/macOS with g++:

    g++ -std=c++17 -O2 -Wall -Wextra -Werror -Iapps/orcsdr-tab5/ui \
      tests/dead_air_model_tests.cpp apps/orcsdr-tab5/ui/dead_air_model.cpp \
      -o /tmp/dead-air-model-tests
    /tmp/dead-air-model-tests

On Windows in a PowerShell session with g++ on PATH:

    g++ -std=c++17 -O2 -Wall -Wextra -Werror -Iapps/orcsdr-tab5/ui `
      tests/dead_air_model_tests.cpp apps/orcsdr-tab5/ui/dead_air_model.cpp `
      -o ./dead-air-model-tests.exe
    ./dead-air-model-tests.exe

Native Tab5 firmware must use the pinned native build script, not PlatformIO:

    Set-Location apps/orcsdr-tab5
    ./tools/build-tab5-idf.ps1

Do not flash any device until the user explicitly authorizes flashing.

## On-device acceptance checklist

- Overview/Spectrum/Audio/Environment/Events/Review all readable at 1280x720 on the physical 5-inch display, all touch targets usable.
- App navigates in and out without drawing after screen ownership passes to another dashboard. Header status region stays clear and global Home/volume/settings work.
- Unplug SDR: no fake spectrum or 0 dBm. Reconnect: actual frequency and relative dBFS metrics only; no phantom data during tuner stabilization.
- ENV III missing: temperature/RH/pressure -- and normal core operation. Present: real current readings; unplug: old values suppressed; stale and warming-up distinguished.
- Test sustained valid RF increases, manual event marking, sensitivity presets, retuning, gain revision changes, cooldown and bounded event overflow.
- After journaling integration, verify session files recover across interrupted writes and that USB audio continues smoothly while recording.
- Record exact branch/commit, toolchain output, test harness result, hardware port, and all deviations in the release notes.

## Verification status

GitHub source review and Git tree generation: performed in web sandbox.
Local native IDF build: NOT RUN (SDK/toolchain unavailable).
Host g++ test: NOT RUN against cloned repository here (container cannot reach github.com).
Physical touchscreen, RTL-SDR, optional ENV III, microphone and SD validation: NOT RUN.
No claim of hardware readiness is made.
