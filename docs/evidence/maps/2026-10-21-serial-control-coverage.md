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
| Open setup / inventory / location picker | Settings action via UI tap/key | Named commands missing. Use the existing map-screen open lifecycle, including reception pause. |
| Picker pan, zoom, recenter, proposed-coordinate entry, Save/Cancel | UI tap/key routing | Named commands and observable proposal/saved-state result needed. |
| Per-consumer Labels ON/OFF | UI tap/key routing | Add a named command using map preferences and correct UI invalidation; preserve independent persistence. |
| Select/rescan a local OrcMaps pack | UI tap/key routing | Named commands needed; unsupported legacy entries must stay unselectable. |
| Check/download/retry/cancel an OrcMaps catalog pack | UI tap/key routing through existing download API | Named commands needed; use the same worker, trust, journal and scheduling gates. |
| ADS-B map range and later LoRa follow/pan/zoom | UI tap/key routing | Named commands needed, shared with the physical control handlers. |

The older `RTL_CATALOG_*` commands control generic signed data packs, not the
OrcMaps download catalog. Do not claim these already provide map-install control.
The retired regional ORCMAP1 entry is absent from their public listing and cannot
be installed; existing SD data remains preserved.

Next implementation milestone: a narrow map/setup command dispatcher connected
to existing shared actions, with bounded parser/authentication tests and explicit
status/error responses. Do not create parallel location, camera, download or
reception state. Add each command to help and its user documentation, then verify
physical/serial behavior against the same saved state on COM17.
