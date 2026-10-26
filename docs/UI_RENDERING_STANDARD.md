# OrcSDR screen rendering standard

The user requires flicker prevention to be a normal coding practice, not a defect
they must rediscover on each new page. Apply this standard to new and modified UI.
It does not claim that every historical screen already satisfies it.

## Render layout once, update changed values

Screen entry establishes the layout, static borders, heading, buttons and hit
regions. Ordinary receiver data, download bytes, elapsed time, cursor movement
and animation update only their affected widgets. Do not call `fillScreen()` or
redraw an entire page because a generic state/revision counter changed.

Separate structural changes (entering a different page, list contents, controls
becoming available) from value-only changes (byte counts, signal level, a progress
bar). A layout transition may redraw its layout. Progress-only and verification
animation updates must keep the existing layout visible. Ignore background
progress when it has no visible effect on the active page.

Each partial region must cover the old content and the new content without
erasing adjacent controls, footer, attribution or overlays. Use opaque text or
clear only the widget's bounded rectangle. Clip moving graphics. If a small
widget still flickers, use an existing appropriately sized sprite/cache after
measuring memory and transfer costs; do not allocate full-frame buffers per tick.

## Ownership and slow operations

The owning UI task draws. Workers publish bounded state/snapshots and completion
revisions; they never draw over the currently active screen. Preserve existing
screen-controller ownership, framebuffer protections and modal/input handling.

Keep the previous completed map/image visible until its replacement is ready.
Paint immediate press/saving/working feedback before NVS, SD or network work.
Progress updates must not steal focus, reset scrolling or rebuild touch targets.
Serial actions share the same handlers, validation and observable result as UI.

## Required validation for new or changed redraw behavior

1. Use the recording display or a focused equivalent. Change data/progress several
   times and assert both the updated visible value and unchanged full-screen-clear
   count. Include unchanged animation ticks and background progress on another
   view if that state is shared.
2. Verify phase/layout transitions still update controls and clear obsolete text.
   Verify navigation, clipping, overlay ownership and immediate button feedback.
3. Run only the affected host tests and native syntax/build checks warranted by
   the change. Save exact commands and actual results. A successful host test does
   not prove that the physical display is flicker-free.
4. On authorized hardware, exercise the actual screen under representative load.
   Record physical acceptance, frame/update timing where relevant, memory/stack,
   and radio continuity separately. Never claim smoothness from desktop timing.

## Known example: setup Maps page

The October 22 download pilot exposed a full-page clear at every download revision
and every verification animation tick. The correction updates the 1216×44 status
region and leaves the layout intact; the Installed view ignores byte-only changes.
The regression changes bytes repeatedly and verifies text updates without another
full-screen clear. This is the expected pattern for future pages.

Related practices: [SD reliability](SD_CARD_RELIABILITY.md) and
[map serial commands](MAP_SERIAL_COMMANDS.md). Existing unrelated UI gaps should
be recorded and addressed in scoped changes, not a broad rendering rewrite.

## IQ continuity is a release invariant

UI and maps must not cause IQ loss. Compare both the driver overrun/drop counters
and application IQ-pipeline drop counters before and after each load phase.
Opening, dragging, zooming, changing labels, storage activity and navigation must
add zero drops under the recorded receiving configuration. A healthy idle period,
a responsive screen, or zero driver drops alone is insufficient. Never suppress
counters, reduce the requested receiver rate, stop reception outside an explicitly
paused workflow, or waive missing samples to make this gate pass.

Large framebuffer writebacks must release the cache synchronization mutex and
interrupt critical section between bounded chunks. The app supplies M5GFX's weak
writeback hook with a 4 KiB limit per `esp_cache_msync` call. SDK inner chunking
alone retains the outer cache mutex for the whole image. Keep the app hook, its
native strong-symbol check and its focused coverage when upgrading M5GFX/IDF.
`RTL_MAP STATUS` exposes `ORC_DISPLAY_WRITEBACK`; its counters supplement rather
than replace the radio continuity counters. Hardware acceptance remains required.
