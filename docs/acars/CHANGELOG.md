# ACARS Changelog

This file tracks ACARS dashboard and decoder changes. The repository has no repository-wide CHANGELOG.md.

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
