# Maps and setup over serial

`RTL_MAP HELP` lists named map commands. Commands are uppercase. Read-only help,
status and lists do not require authentication. Every operation that changes
state requires the existing authenticated serial session; this interface never
pairs a new key. Responses end in a newline.

Keep the session alive with `PING` at intervals shorter than five seconds,
including while waiting for a command result. A location save can take several
seconds; wait for its final result rather than assuming a short silence means
failure. Reception restart after leaving setup is asynchronous: allow it to
settle, then query `RTL_DRIVER STATUS`.

Successful operations return `RTL_MAP_OK`. Errors are `auth_required`,
`invalid_command`, `busy` or `rejected`. A rejected operation can mean the
wrong screen/step, an active input editor, a camera limit, unavailable storage,
a failed save or an installer gate. Query status for context. An acknowledgement
does not mean an asynchronous render or download has finished.

## Inspect state

```text
RTL_MAP STATUS
RTL_MAP LIST
RTL_MAP DOWNLOAD LIST
```

Status reports the active picker proposal separately from the saved receiver
location, along with existing map/render/install diagnostics. View modes are
0=picker, 1=inventory, 2=setup; setup steps are 0=Welcome, 1=Network, 2=Location,
3=Maps, 4=Complete. Coordinates are signed E7 integers: degrees × 10,000,000.
`ORC_MAP_SOURCE` distinguishes camera zoom from stored tile zoom.

Local pack listings include the full identity and usability. Preserved older
ORCMAP1 files are unsupported and cannot be selected. Download lists describe
the distinct OrcMaps catalog; `RTL_CATALOG_*` remains the generic data catalog.

## ADS-B map range

```text
RTL_ADSB_RANGE
RTL_ADSB_RANGE 25
RTL_UI ACTION SETTINGS RANGE 50
```

The first command is read-only. Both setters require authentication and use the
shared range action, including an open dashboard and deferred persistence.
Supported radii are 10, 25, 50 and 100 nautical miles. The dedicated setter
accepts exactly one of these numbers; it rejects extra tokens and malformed or
overflowing numbers. This does not add radar rings or a crosshair.

`RTL_ADSB_RANGE_STATUS` reports `range_nm`, `dashboard_range_nm`, `active` and
`pending`. The dashboard value describes its retained state; compare it with
the configured value when `active=1`. `RTL_ADSB_RANGE_OK ... persistence=pending`
acknowledges the action, not a durable NVS write or a completed render. Observe
the settings-save result and a fresh frame, then verify persistence separately.

## Location picker

```text
RTL_MAP OPEN PICKER
RTL_MAP PICKER SET 100000000 200000000
RTL_MAP PICKER ZOOM IN
RTL_MAP PICKER PAN 40 -20
RTL_MAP STATUS
RTL_MAP PICKER SAVE
```

`SET` proposes a location; it does **not** save it. The example is 10°N, 20°E,
not a shipping default. `PAN` moves the camera east/south in screen pixels;
negative values move west/north. Each request is bounded to ±1280 horizontally
and ±720 vertically. `ZOOM` accepts `IN` or `OUT` and respects picker limits.
These operations require an active picker, or its location step in setup.

`SAVE` uses the same Saving feedback and verified location hook as the button.
It returns from the ordinary picker or advances setup only after a successful
save. `RTL_MAP CANCEL` discards the unsaved proposal and closes the map surface.
It does not stop an independently running map download.

## Local packs and labels

```text
RTL_MAP OPEN LIST
RTL_MAP LIST
RTL_MAP SELECT <full-pack-identity>
RTL_MAP RESCAN
RTL_MAP LABELS PICKER OFF
RTL_MAP LABELS LORA ON
RTL_MAP LABELS ADSB ON
RTL_MAP LABELS FT8 OFF
```

Selection requires the local inventory or setup's Maps step, and a usable
listed identity. Selection follows the same storage and navigation hook as
the button. Rescan uses the existing worker and recovery path.

Label preferences are independent and persist across reboot. Their commands
work without opening the corresponding dashboard. A saved change invalidates
the shared map presentation so active consumers repaint. Names still depend on
the map's actual contents and the city-label zoom threshold.

## Setup

```text
RTL_MAP OPEN SETUP
RTL_MAP SETUP NEXT
RTL_MAP SETUP SKIP
RTL_MAP SETUP BACK
RTL_MAP SETUP QUICK
RTL_MAP SETUP FINISH
RTL_MAP CANCEL
```

The wizard uses its existing reception pause/restore lifecycle. A failed pause
rejects opening. `NEXT` and `SKIP` advance one step; `BACK` also handles a location
subview/input. `QUICK` is available on Welcome. `FINISH` is available only on
Complete and saves the setup record before closing. Failed progress writes do
not advance an automatic setup, and failed Finish keeps the wizard open.

Network provisioning and address/network location lookup keep their existing
`RTL_WIFI_*` and `RTL_LOCATION IP|LOOKUP <query>|CONFIRM` commands. This document
does not claim a complete application-wide command audit.

## Map downloads

```text
RTL_MAP DOWNLOAD CHECK
RTL_MAP DOWNLOAD LIST
RTL_MAP DOWNLOAD INSTALL <index>
RTL_MAP DOWNLOAD CANCEL
RTL_MAP DOWNLOAD RETRY
```

Use a zero-based index from the current download list. Check, install, cancel
and retry use the same installer APIs and eligibility checks as the UI. Already
installed entries are not reinstalled through this command. Nothing bypasses
catalog trust, archive verification, activation/recovery or storage checks.

The production catalog URL/key are currently unconfigured. A rejected Check or
empty list in that state is expected; these commands do not provide a new host,
publish assets or prove an end-to-end HTTPS installation.
