# Label integration — native build attempt

October 18, 2026. User approved one full native build and inspection of
`8d1eaa48` plus preserved wizard edits, using OrcMaps `5f3a1eb`.
No flash, erase, C6 update, push or merge was authorized or performed.

## Actual result: build failed before linking

The first configuration attempt lacked the cached RISC-V compiler directory
in PATH. After correcting PATH, the build regenerated configuration without
the previous SDKCONFIG/cache inputs and failed compiling ESP-Hosted Wi-Fi
initialization due to missing CONFIG_WIFI_RMT_* symbols. Explicitly selecting
the existing Hosted-3 sdkconfig and C6 image did not resolve those missing
symbols. No new firmware size or PSRAM-placement result can be claimed.

Logs are saved in the directory from which commands ran:
`apps/orcsdr-tab5/label-native-config-failed-20261018.log`,
`apps/orcsdr-tab5/label-native-build-20261018.log`, and
`apps/orcsdr-tab5/label-native-build-configured-20261018.log`.
The first log was moved there after initially being redirected outside that
directory. No logs or recovery artifacts were deleted.

## Narrow diagnosis

ESP-WiFi-Remote's Kconfig conditionally loads
`Kconfig.idf_v$ESP_IDF_VERSION.in`. The build environment lacked
ESP_IDF_VERSION; generated config.env supplied IDF_VERSION=5.5.4, a different
variable. The component's version-specific buffer options were omitted.

Copied sdkconfig to a local probe file, then ran only configuration generation:

```
C:/Espressif/python_env/idf5.5_py3.14_env/Scripts/python.exe -m kconfgen --kconfig C:/Espressif/frameworks/esp-idf-v5.5.4/Kconfig --config label-kconfig-probe.config --env-file build-native-hosted3/config.env --list-separator semicolon --env ESP_IDF_VERSION=5.5 --output header label-kconfig-probe.h
```

This focused probe passed and generated:

- CONFIG_WIFI_RMT_STATIC_RX_BUFFER_NUM=16
- CONFIG_WIFI_RMT_DYNAMIC_RX_BUFFER_NUM=32
- CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL=40960

Log: `apps/orcsdr-tab5/label-kconfig-probe-fixed-20261018.log`.
The earlier probe omitted the list separator and failed; its log is preserved.
This establishes the missing-option cause, not a successful firmware build.
No managed component source or Wi-Fi implementation was patched.

## Next gate

Request one corrected native build with explicit ESP_IDF_VERSION=5.5,
SDKCONFIG, existing C6 image, toolchain PATH and startup diagnostics disabled.
Then inspect firmware size, label .ext_ram.bss placement, DMA reserve,
requested/locked/fetched pin, miniz symbols and FT8 scratch placement.
The prepared local inspection script has not run because linking did not finish.
Neither Tab5 has changed. Hardware validation remains pending.
