# Map and setup serial-control coverage

User requirement, October 21: everything available to an OrcSDR user must have
a serial-command equivalent. This is a bounded source inventory of maps/setup,
not a completed application-wide audit or a claim that every route was tested.

| User operation | Current serial route | Remaining gate |
| --- | --- | --- |
| Inspect map availability, labels, rendering and installer state | `RTL_MAP_STATUS` | Source-camera diagnostic validated on COM17 with `b326a123`; full global-pack device gate is separate. |
| Open a dashboard | `RTL_UI OPEN ADSB`, `LORA`, `FT8`, `SETTINGS`, etc. | Existing authenticated route; check the actual screen owner, not acknowledgement alone. |
| Tap a visible control / enter a key | `RTL_UI TAP x y`, `RTL_KEYBOARD KEY ...` | Useful for UI-route regression; does not replace discoverable semantic map commands. |
| Set receiver lat/lon | `RTL_ADSB_LOCATION <latitude> <longitude>` | Existing shared location save route; command naming and authentication parity need audit. |
| Resolve approximate network location | `RTL_LOCATION IP`, then `RTL_LOCATION CONFIRM` | Existing lookup/confirm route; persistence and failure gates remain scoped. |
| Resolve an address or ZIP | `RTL_LOCATION LOOKUP <query>`, then `RTL_LOCATION CONFIRM` | Existing lookup/confirm route. |
| Inspect lookup state | `RTL_LOCATION STATUS` | Reports lookup result; add a clear saved receiver/map-camera status if not already exposed elsewhere. |
| Open setup / inventory / location picker | `RTL_MAP OPEN SETUP\|LIST\|PICKER` | Shared screen lifecycle, including reception pause; bounded COM17 ownership checks pass. Full setup interruption/recovery remains open. |
| Picker pan, zoom, proposed-coordinate entry, Save/Cancel | `RTL_MAP PICKER PAN`, `ZOOM`, `SET`, `SAVE`; `RTL_MAP CANCEL` | COM17 proposal/cancel and same-point save pass. Status distinguishes proposal from saved E7 coordinates. |
| Setup Back, Next, Skip, Quick Start, Finish | `RTL_MAP SETUP BACK\|NEXT\|SKIP\|QUICK\|FINISH` | Focused host coverage; COM17 traversed all five steps with optional pages skipped. All-method setup acceptance remains open. |
| Per-consumer Labels ON/OFF | `RTL_MAP LABELS PICKER\|LORA\|ADSB\|FT8 ON\|OFF` | Shared preferences and invalidation; independent persistence covered on host, ADS-B preference OFF/ON checked on COM17. Other physical consumer checks remain scoped. |
| Select/rescan a local OrcMaps pack | `RTL_MAP SELECT <full-id>`; `RTL_MAP RESCAN` | Unsupported/unknown selection rejection passes on COM17. `ba0e7a92` rescan refresh advances displayed frame revision 2→4. Successful selection persistence remains a separate gate. |
| Check/download/retry/cancel an OrcMaps catalog pack | `RTL_MAP DOWNLOAD CHECK\|INSTALL <index>\|RETRY\|CANCEL` | Shared worker/trust/journal gates; host coverage and unconfigured-catalog rejection on COM17. Actual HTTPS installation remains open. |
| ADS-B map range and later LoRa follow/pan/zoom | UI tap/key routing | Named commands needed, shared with the physical control handlers. |

The older `RTL_CATALOG_*` commands control generic signed data packs, not the
OrcMaps download catalog. Do not claim these already provide map-install control.
The retired regional ORCMAP1 entry is absent from their public listing and cannot
be installed; existing SD data remains preserved.

The dispatcher landed on the development branch in `1cd9b180`, with focused
optimized and sanitizer checks passing. See [command reference](../../MAP_SERIAL_COMMANDS.md)
and [device evidence](2026-10-21-map-serial-controls.md). This closes the named
map/setup dispatcher implementation gap, not application-wide parity or all
hardware acceptance. ADS-B range and future LoRa interactions still need shared
semantic controls.
