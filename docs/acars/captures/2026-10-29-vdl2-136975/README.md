# Live VDL Mode 2 capture, 2026-10-29, Tab5 on 136.975 MHz

Receiver: OrcSDR Tab5 (COM17), native VDL2 decoder, 250 kS/s stream. About 06:23 to 06:35 UTC. Every message below is a CRC-valid AVLC
frame carrying ACARS. Screenshots are the message detail pages (`message-00.png` is the newest, `message-09.png` the oldest);
`live-list.png` is the LIVE list and `channels-page.png` the channel list with the new VDL2 preset.

Device counters at the time (`RTL_ACARS STATUS`): 18 CRC-valid AVLC frames (10 ACARS, 8 with no data), 34 valid VDL2 headers,
25 error-correction passes, 1 rejected frame, no dropped messages, no stream gaps.

## Messages (text exactly as received)

| # | UTC | Tail | Label | Block | Text |
|---|---|---|---|---|---|
| 00 | 06:34:54 | N244SY | H1 | 1 | `C87AOO137C#CFBFDE0633591029ABZ 06315707301    0    0 06315707308    0    0 06320507310    0    0` |
| 01 | 06:34:09 | N244SY | H1 | 1 | same as 00 (resent) |
| 02 | 06:33:55 | N338DU | Q0 | 9 | `S50ADL0661` |
| 03 | 06:33:45 | N244SY | 32 | 0 | `M19AOO137CP,E,2026-10-29 06:33:43,3870,65654,N 44.221 W 123.107,3723,232,011,3,163,171,263,0636,` |
| 04 | 06:31:45 | N244SY | 32 | 9 | `M18AOO137CP,E,2026-10-29 06:31:43,3920,65704,N 44.204 W 122.970,5491,246,017,0,204,217,337,0636,` |
| 05 | 06:30:28 | N244SY | H1 | 8 | `C86AOO137C#CFBFDE0630151029ABZ 06281417294    0    0 06281517402    0    0 06283617310    0    0 06283617301    0    0 06283617308    0    0 06295107402    0    0` |
| 06 | 06:30:25 | N244SY | H1 | 7 | same as 05 with message number C85 |
| 07 | 06:29:45 | N244SY | 32 | 6 | `M17AOO137CP,E,2026-10-29 06:29:43,3970,65754,N 44.182 W 122.794,7321,235,023,-4,254,270,423,0636,` |
| 08 | 06:25:45 | N244SY | 32 | 4 | `M15AOO137CP,E,2026-10-29 06:25:43,4080,65864,N 44.102 W 122.399,11581,249,029,-9,274,297,468,0636,` |
| 09 | 06:23:45 | N244SY | 32 | 9 | `M10AOO137CP,E,2026-10-29 06:23:43,4140,65924,N 44.045 W 122.171,14097,249,045,-13,333,368,585,0635,` |

Flight OO137C is SkyWest. The label-32 track descends toward Eugene, Oregon (the ground station that sends squitters on this channel is
EUG at 44.117N 123.217W).

## What the interpreter decodes now, and what it does not

- Label 32: date, time and position are decoded (the time matches the receive time to two seconds). The ten other numbers are shown as
  received. Hypotheses from the five samples, **not verified**: 14097/11581/7321/5491/3723 fall like an altitude in feet on the descent;
  249/249/235/246/232 look like a speed in knots; -13/-9/-4/0/3 rise as the aircraft descends, like an outside air temperature in
  degrees C; 4140 and 65924 move together (65924 - 4140 = 61784 in every sample), which fits fuel on board and gross weight in pounds;
  045/029/023/017/011 fall toward zero near the destination (not simple distance to the airport). Nothing documents these.
- `H1 #CFB FDE...` (central fault display event list): structure decoded (report time, `1029ABZ` tag, entries of hhmmss + five digit
  code + two zero numbers). Codes are shown as received. In the samples the same four-digit codes appear with a leading `1` at
  06:28:36 (7310, 7301, 7308) and with a leading `0` at 06:31:57 to 06:32:05, and 7402 appears as `17402` at 06:28:15 and `07402`
  at 06:29:51, which suggests the leading digit is on/off. 7294 is only seen with `1`. Not verified.
- `Q0`: link test, decoded.

## Cross-reference with web sources (2026-10-29)

| Check | Source | Result |
|---|---|---|
| Tail N244SY is a SkyWest Embraer ERJ 170-200 | FAA registry (Mode S hex A24082) | confirmed |
| Flight OO137C is callsign SKW137C | OpenSky flights API for A24082 | confirmed: callsign `SKW137C`, departed KSLC (first seen 04:55:13Z), last seen 06:38:15Z |
| Last seen a few minutes after our last report (06:34Z), near Eugene | OpenSky | consistent with landing at Eugene (EUG) |
| Delta link test: N338DU, flight DL0661 | adsb.lol live feed | confirmed: N338DU is `DAL661` (BCS3), airborne over Oregon at 37,875 ft |
| Label 32 track ends at Eugene (EUG 44.1246N 123.2120W) | our five positions | distance falls 45.1, 35.1, 18.3, 11.5, 7.3 nm |
| Field after the position is altitude in feet | community note ("altitude in feet?") and our data | descent rates between reports 1258, 1065, 915, 884 fpm while closing on EUG: consistent |
| The speed-like field (249, 249, 235, 246, 232) is ground speed | position legs | **no**: implied ground speed is 312, 265, 231, 180 kt; the field is something else |
| `FDE` is Flight Deck Effect; four digits after the time are month-day | community CFB page and our samples | consistent (`1029` = 29 Oct; community `0805` beside a 2020-08-05 report) |

Not done: the position track of N244SY from a public source. Anonymous OpenSky track and state history endpoints return 404 or need an
account, and adsb.lol/airplanes.live trace files return 403 to scripts. `tools/acars_adsb_xref.py` compares a report with the live
adsb.lol position, so it only works within a minute or two of the report. Credentials for any research account stay in environment
variables on the researcher's own machine and never go into code.
