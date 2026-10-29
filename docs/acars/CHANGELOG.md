# ACARS Changelog

This file tracks ACARS dashboard and decoder changes. The repository has no repository-wide CHANGELOG.md.

## 2026-10-29 (later) - Live VDL2 captures and two new formats
- First live VDL2 traffic on the Tab5 (SkyWest N244SY on OO137C, Delta N338DU on DL0661): 18 CRC-valid AVLC frames, 10 ACARS, no drops.
- Interpreter: label 32 position reports (date, time, position; altitude marked probable; other values as received) and `H1 #CFB FDE`
  flight-deck-effect lists (report time and date, entries as received). Captures, screenshots and the cross-check against FAA,
  OpenSky and adsb.lol are in `captures/2026-10-29-vdl2-136975/`.
- `tools/acars_adsb_xref.py` compares a decoded position with the live adsb.lol position.
- Decoder CPU on the Tab5 is about 28 ms per 65 ms block when idle (44% of a core). Likely cause: double-precision arithmetic in
  `vdl2_receiver.cpp` (the P4 has a single-precision FPU only). Not changed yet.
- Quiet-hour run: 30 minutes on 136.975 MHz from 07:03Z with no messages; light traffic.

## 2026-10-29 - VDL Mode 2 on the ACARS dashboard (flashed to COM17, live RF not yet heard)

- **Receive path chosen by channel:** a requested channel from 136.725 to 136.975 MHz runs the native `vdl2::Channel` (250 kS/s stream,
  channel 62.5 kHz above the tuned centre, /5 to 50 kS/s, D8PSK, Reed-Solomon FEC, AVLC) instead of the 240 kS/s analog front end.
  The analog receiver is unchanged. `Hooks::tune` now takes the stream rate, so `main.cpp` got smaller, not larger.
- **Records:** ACARS inside AVLC frames posts like an analog block. Other data frames (ATN, CPDLC, ADS-C) are kept as raw hex under
  the label `AV` with the 28-bit source address as the tail and are shown, not interpreted. Frames with no data are only counted.
- **Channels page:** a `136.975 VDL2` preset in every region; the list now has seven 48 px rows. `RTL_ACARS STATUS` gained `vdl2`,
  `v2_hdr_ok`, `v2_fec_ok`, `v2_acars`, `v2_raw`, `v2_other`.
- **Evidence:**
  - the native decoder passes the xng off-air fixture (`vdl2_offair_6s.i16`, 6 s at 50 kS/s): 4 CRC-valid AVLC frames, including
    ACARS from HB-IJW (label B9, then a label `_?` acknowledgement), under ASan/UBSan;
  - the dashboard adapter passes a synthetic-IQ test (ACARS frame, raw frame, counted frame, switch back to analog);
  - on the Tab5 the 136.975 preset tunes, the stream runs at 250 kS/s with no wrong-rate blocks and no gaps.
- **Measured on the Tab5 (idle channel, 15 blocks/s of 32 KB = 65 ms of signal each):** the decoder takes 22 ms on average per
  block (about a third of one core) with a worst case of 81 ms, which is longer than one block. Internal RAM free 59.9 KB (min 42.6 KB);
  the receiver state lives in PSRAM. USB throughput was not stressed separately.
- **Not verified:** any live VDL2 frame on the device or the 60 s indoor capture from a second dongle (no energy bursts above the noise,
  7% of samples clipped); a dumpvdl2 cross-check (not built: needs an external build); BER/SNR sweep; collision behaviour.

## 2026-10-28 - Application-text interpreter (host-tested, not flashed)

- **New `acars_interpret.{hpp,cpp}`:** plain-language reading of squitters, acknowledgements, autotune, link tests, H1 sub-labels,
  FMC position reports (`#M1BPOS`), label-21 position reports, and flight-data-unit (`#DF`) cruise fixes, `TRP` blocks and route
  headers, plus names for about 50 labels and airline names for the common US prefixes. Details, sources and cross-checks:
  `INTERPRETER.md`.
- **Dashboard:** the message detail page's APPLICATION INTERPRETATION panel shows the decoded headline and lines (it used to always say
  "NOT SUPPLIED"); the LIVE list shows a cyan interpreted summary for decoded blocks and keeps raw text for everything else. No new
  per-record storage.
- **Tests:** `tests/acars_interpret_tests.cpp` uses blocks captured from the Tab5 on 131.550 MHz, with a hostile-input loop under
  ASan/UBSan, and `tests/acars_dashboard_render_tests.cpp` checks list and detail drawing. `tools/test-acars-ui.sh` passes in full.
- Not yet built with ESP-IDF or run on a device.
