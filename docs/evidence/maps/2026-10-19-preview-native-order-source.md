# Native-order preview experiment — October 19, 2026

Measured checkpoint: physical Tab5 target enum272 (rgb565_nonswapped), immutable
preview enum16 (rgb565_2Byte), internalrotation3. Hypothesis: convert the completed
snapshot once during worker publication, then remove byte swapping from repeated
preview paints. This does not remove rotated copy/scaling and may not improve
hardware timing. Keep only if correctness holds and measured costs justify it.

Only map_service.cpp publication allocation changes: setColorDepth receives the
supported lgfx::rgb565_nonswapped enum. Existing g_canvas.pushSprite converts into
that format using M5GFX. Bytes/pixel remain2; lock/lifetime/reclaim/fallback and
all UI/framebuffer/orientation ownership remain unchanged. No managed edits,
raw buffers or DMA calls. Worker canvas stays16 and completed-frame push path
is unchanged. Other targets still receive conversion via the existing API.

Actual pinned M5GFX pixelcopy.cpp compiled with standalone host test:
apps/orcsdr-tab5/tests/maps/map_preview_format_tests.cpp. No implementation copy.
Exhaustive65536 source values prove expected byte swap, old-direct vs
preconverted sampling match for forward/transposed/reversed/fractional traversal,
output guard words intact, and reverse-format destination remains correct.
No desktop speed claim. One configured test PASS1/1.

Run from integration root:

    cmake -S .local/maps/preview-format-host-20261019 -B .local/maps/preview-format-host-20261019/build
    cmake --build .local/maps/preview-format-host-20261019/build --config Debug
    ctest --test-dir .local/maps/preview-format-host-20261019/build -C Debug --output-on-failure
    python .local/maps/check_preview_format_syntax_20261019.py

Logs preview-format-{configure,build,tests,p4-syntax}-20261019.log.
P4 syntax PASS with existing SDK literal-suffix warning. No native/hardware
speed or pixel acceptance yet. This is a bounded performance experiment under
the user's current-session build/COM17 test authority. No publication/merge.
