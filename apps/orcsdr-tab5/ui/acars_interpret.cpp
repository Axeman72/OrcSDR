// SPDX-License-Identifier: AGPL-3.0-only
#include "acars_interpret.hpp"

#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace orcsdr::acars_ui {
namespace {

#if defined(__GNUC__)
#define ACARS_PRINTF(a, b) __attribute__((format(printf, a, b)))
#else
#define ACARS_PRINTF(a, b)
#endif

bool is_digit(char c) { return c >= '0' && c <= '9'; }
bool is_upper(char c) { return c >= 'A' && c <= 'Z'; }

bool all_digits(const char* p, size_t n) {
  for (size_t i = 0; i < n; ++i)
    if (!is_digit(p[i])) return false;
  return n > 0;
}
unsigned dec(const char* p, size_t n) {
  unsigned v = 0;
  for (size_t i = 0; i < n; ++i) v = v * 10u + static_cast<unsigned>(p[i] - '0');
  return v;
}

// Bounded view of the block text; every read past the end yields NUL.
struct Text {
  const char* p;
  size_t n;
  char at(size_t i) const { return i < n ? p[i] : '\0'; }
  bool has(size_t i, const char* lit) const {
    const size_t len = std::strlen(lit);
    return i + len <= n && std::memcmp(p + i, lit, len) == 0;
  }
};

void add_line(Interpretation& r, const char* fmt, ...) ACARS_PRINTF(2, 3);
void add_line(Interpretation& r, const char* fmt, ...) {
  if (r.line_count >= kInterpLines) return;
  va_list ap;
  va_start(ap, fmt);
  std::vsnprintf(r.lines[r.line_count], kInterpLineChars, fmt, ap);
  va_end(ap);
  ++r.line_count;
}
void set_headline(Interpretation& r, const char* s) { std::snprintf(r.headline, sizeof(r.headline), "%s", s); }
void set_summary(Interpretation& r, const char* fmt, ...) ACARS_PRINTF(2, 3);
void set_summary(Interpretation& r, const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  std::vsnprintf(r.summary, sizeof(r.summary), fmt, ap);
  va_end(ap);
}

void fmt_hms(char* out, size_t cap, const char* six) {   // "202750" -> "20:27:50Z"
  std::snprintf(out, cap, "%c%c:%c%c:%c%cZ", six[0], six[1], six[2], six[3], six[4], six[5]);
}
bool valid_hms(const char* six) {
  return dec(six, 2) < 24 && dec(six + 2, 2) < 60 && dec(six + 4, 2) < 60;
}

// Latitude / longitude as signed-hemisphere decimal degrees with three decimals, e.g. "44.662N".
void fmt_coord(char* out, size_t cap, char hemi, unsigned milli_deg) {
  std::snprintf(out, cap, "%u.%03u%c", milli_deg / 1000u, milli_deg % 1000u, hemi);
}
// Degrees plus minutes with one decimal ("44397" = 44 deg 39.7 min). `deg_digits` is 2 for latitude, 3 for longitude.
bool parse_degmin(const char* p, size_t deg_digits, unsigned* milli_deg) {
  if (!all_digits(p, deg_digits + 3)) return false;
  const unsigned deg = dec(p, deg_digits);
  const unsigned tenths = dec(p + deg_digits, 3);   // minutes * 10
  if (tenths >= 600) return false;
  *milli_deg = deg * 1000u + (tenths * 5u + 1u) / 3u;
  return true;
}
// Plain decimal degrees ("42.872", "044.6863") truncated to thousandths; sets *negative for a leading '-'.
bool parse_decimal(const char* p, size_t n, unsigned* milli_deg, bool* negative) {
  size_t i = 0;
  *negative = false;
  if (i < n && p[i] == '-') { *negative = true; ++i; }
  unsigned whole = 0;
  size_t d = 0;
  while (i < n && is_digit(p[i])) { whole = whole * 10u + static_cast<unsigned>(p[i] - '0'); ++i; ++d; }
  if (d == 0 || d > 3) return false;
  unsigned frac = 0;
  size_t fd = 0;
  if (i < n && p[i] == '.') {
    ++i;
    while (i < n && is_digit(p[i])) {
      if (fd < 3) { frac = frac * 10u + static_cast<unsigned>(p[i] - '0'); ++fd; }
      ++i;
    }
  }
  while (fd < 3) { frac *= 10u; ++fd; }
  if (i != n) return false;
  *milli_deg = whole * 1000u + frac;
  return true;
}

void fmt_alt(char* out, size_t cap, unsigned hundreds) {   // altitude in hundreds of feet
  if (hundreds >= 180u) std::snprintf(out, cap, "FL%03u", hundreds);
  else std::snprintf(out, cap, "%u ft", hundreds * 100u);
}
void fmt_alt_ft(char* out, size_t cap, unsigned feet) {
  if (feet >= 18000u) std::snprintf(out, cap, "FL%03u", (feet + 50u) / 100u);
  else std::snprintf(out, cap, "%u ft", feet);
}
// "M57" -> -57, "P12" -> 12, "-59" -> -59, "-58.0" -> -58 (tenths dropped).
bool parse_temp(const char* p, size_t n, int* celsius) {
  if (n < 2) return false;
  bool neg = false;
  size_t i = 0;
  if (p[0] == 'M' || p[0] == '-') { neg = true; i = 1; }
  else if (p[0] == 'P' || p[0] == '+') i = 1;
  size_t d = 0;
  int v = 0;
  while (i < n && is_digit(p[i])) { v = v * 10 + (p[i] - '0'); ++i; ++d; }
  if (d == 0 || d > 3) return false;
  if (i < n) {
    if (p[i] != '.') return false;
    for (++i; i < n; ++i)
      if (!is_digit(p[i])) return false;
  }
  *celsius = neg ? -v : v;
  return true;
}

// ---- tables ---------------------------------------------------------------------------------------------------
struct LabelRow { char a, b; const char* name; };
// Names come from ARINC 620 Table C-1 where the row is unambiguous, otherwise from the community label research. Our own words.
constexpr LabelRow kLabels[] = {
    {'_', '?', "General response (acknowledgement)"},
    {'_', 'd', "Link ping (demand mode)"},
    {'Q', '0', "Link test"},
    {'S', 'Q', "Ground station squitter"},
    {':', ';', "Radio autotune (frequency change)"},
    {':', '}', "Retune to VDL"},
    {'H', '1', "Message to/from an on-board unit"},
    {'5', 'Z', "Airline-defined downlink"},
    {'5', 'R', "Crew-initiated position report"},
    {'5', '7', "Crew-initiated position report"},
    {'5', 'U', "Weather request"},
    {'5', 'D', "ATIS request"},
    {'5', 'V', "VDL switch advisory"},
    {'5', 'Y', "ETA revision"},
    {'5', '1', "Ground time request"},
    {'5', '4', "Voice go-ahead / frequency uplink"},
    {'7', 'A', "Engine data / takeoff thrust report"},
    {'7', 'B', "Crew-entered miscellaneous message"},
    {'A', '0', "ATS facility notification (AFN)"},
    {'B', '0', "ATS facility notification (AFN)"},
    {'A', '1', "Oceanic clearance"},
    {'A', '3', "Departure clearance"},
    {'A', '9', "ATIS report"},
    {'B', '1', "Oceanic clearance request"},
    {'B', '2', "Oceanic clearance readback"},
    {'B', '3', "Incomplete message advisory"},
    {'B', '4', "Departure clearance readback"},
    {'B', '6', "ADS report request"},
    {'B', '7', "Free text to ATC"},
    {'B', '8', "Departure slot request"},
    {'B', 'A', "ATC communication"},
    {'B', 'B', "Terminal weather for pilots"},
    {'C', '1', "Cockpit/cabin printer message"},
    {'E', '1', "Internet e-mail message"},
    {'H', 'X', "Undelivered uplink report"},
    {'M', 'A', "Media advisory (link status)"},
    {'Q', '1', "Departure/arrival report"},
    {'Q', '2', "ETA report"},
    {'Q', '3', "Clock update advisory"},
    {'Q', 'A', "OUT report (left the gate)"},
    {'Q', 'B', "OFF report (took off)"},
    {'Q', 'C', "ON report (landed)"},
    {'Q', 'D', "IN report (at the gate)"},
    {'Q', 'E', "OUT report with destination"},
    {'Q', 'F', "OFF report with destination"},
    {'Q', 'K', "Landing report"},
    {'Q', 'L', "Arrival report"},
    {'Q', 'M', "Arrival information report"},
    {'Q', 'N', "Diversion report"},
    {'R', 'A', "Command/response uplink"},
    {'R', 'B', "Command/response downlink"},
    {'S', '1', "VHF network statistics"},
    {'S', '3', "LRU configuration report"},
    {'2', '1', "Position report"},
};

struct NameRow { char a, b; const char* name; };
// H1 sub-labels: which on-board unit a message came from or is for (ARINC 620 Table C-1).
constexpr NameRow kSublabels[] = {
    {'D', 'F', "flight data recorder unit (DFDAU)"},
    {'M', '1', "left FMC"},
    {'M', '2', "right FMC"},
    {'M', '3', "centre FMC"},
    {'M', 'D', "selected FMC"},
    {'C', 'F', "central fault display"},
    {'E', 'I', "engine indicating system"},
    {'W', 'O', "weather observation"},
    {'H', '1', "left HF data radio"},
    {'H', '2', "right HF data radio"},
    {'T', '8', "vendor-defined unit"},
};

struct AirlineRow { char a, b; const char* name; };
constexpr AirlineRow kAirlines[] = {
    {'A', 'S', "Alaska"},     {'F', '9', "Frontier"},   {'W', 'N', "Southwest"}, {'D', 'L', "Delta"},
    {'A', 'A', "American"},   {'U', 'A', "United"},     {'B', '6', "JetBlue"},   {'N', 'K', "Spirit"},
    {'H', 'A', "Hawaiian"},   {'G', '4', "Allegiant"},  {'Q', 'X', "Horizon"},   {'O', 'O', "SkyWest"},
    {'A', 'C', "Air Canada"}, {'F', 'X', "FedEx"},      {'5', 'X', "UPS"},       {'Y', 'X', "Republic"},
    {'M', 'Q', "Envoy"},      {'O', 'H', "PSA"},        {'S', 'Y', "Sun Country"},
};

const char* sublabel_name(char a, char b) {
  for (const auto& r : kSublabels)
    if (r.a == a && r.b == b) return r.name;
  return nullptr;
}

// ---- common pieces ------------------------------------------------------------------------------------------------
// ARINC 620 downlink text starts with a four character message sequence number (originator, 2 digit number, block letter)
// and a six character flight identifier.
struct Header {
  bool ok = false;
  char msn[5]{};
  char flight[7]{};
};
Header parse_header(const MessageRecord& m, const Text& t) {
  Header h;
  if (t.n < 10) return h;
  if (!is_upper(t.at(0)) || !all_digits(t.p + 1, 2) || !is_upper(t.at(3))) return h;
  for (size_t i = 4; i < 10; ++i) {
    const char c = t.at(i);
    if (!(is_upper(c) || is_digit(c) || c == ' ')) return h;
  }
  std::memcpy(h.msn, t.p, 4);
  std::memcpy(h.flight, t.p + 4, 6);
  size_t n = 6;
  while (n > 0 && h.flight[n - 1] == ' ') h.flight[--n] = '\0';
  (void)m;
  h.ok = true;
  return h;
}

void add_header_line(Interpretation& r, const Header& h) {
  if (!h.ok) return;
  const char* airline = airline_name(h.flight);
  const char part = h.msn[3];
  if (airline) add_line(r, "Message %.3s part %c   Flight %s (%s)", h.msn, part, h.flight, airline);
  else add_line(r, "Message %.3s part %c   Flight %s", h.msn, part, h.flight);
}

bool block_is_uplink(char block_id) { return block_id >= 'A' && block_id <= 'Z'; }

// ---- label parsers ------------------------------------------------------------------------------------------------
bool interpret_response(const MessageRecord& m, const Text& t, Interpretation& r) {
  const bool up = block_is_uplink(m.block_id);
  set_headline(r, up ? "GROUND ACKNOWLEDGEMENT" : "AIRCRAFT ACKNOWLEDGEMENT");
  // The acknowledgement character names the block being confirmed. NAK (and any unprintable byte, shown as '?') means none.
  const char a = m.acknowledgement;
  if (is_digit(a)) add_line(r, "Confirms downlink block %c was received.", a);
  else if (is_upper(a)) add_line(r, "Confirms uplink block %c was received.", a);
  else add_line(r, "Technical response, no block named.");
  const Header h = parse_header(m, t);
  if (h.ok) add_line(r, "Message %.4s   Flight %s", h.msn, h.flight);
  if (is_digit(a) || is_upper(a)) set_summary(r, "ACK of block %c%s%s", a, h.ok ? " " : "", h.ok ? h.flight : "");
  else set_summary(r, "Technical response%s%s", h.ok ? " " : "", h.ok ? h.flight : "");
  return true;
}

bool interpret_autotune(const Text& t, Interpretation& r) {
  if (t.n < 6 || !all_digits(t.p, 6)) return false;
  const unsigned khz = dec(t.p, 6);
  set_headline(r, "RADIO AUTOTUNE");
  add_line(r, "Tune the radio to %u.%03u MHz.", khz / 1000u, khz % 1000u);
  if (t.n >= 10 && all_digits(t.p + 6, 4)) add_line(r, "For %u seconds.", dec(t.p + 6, 4));
  set_summary(r, "Retune to %u.%03u MHz", khz / 1000u, khz % 1000u);
  return true;
}

// ARINC 620 uplink squitter: versions 0, 1 and 2.
bool interpret_squitter(const Text& t, Interpretation& r) {
  if (t.n < 4 || !all_digits(t.p, 2)) return false;
  const unsigned version = dec(t.p, 2);
  if (version > 2) return false;
  const char p0 = t.at(2), p1 = t.at(3);
  const char* provider = (p0 == 'X' && p1 == 'A') ? "ARINC" : (p0 == 'X' && p1 == 'S') ? "SITA" : nullptr;
  set_headline(r, "GROUND STATION SQUITTER");
  char who[48];
  if (provider) std::snprintf(who, sizeof(who), "%s", provider);
  else std::snprintf(who, sizeof(who), "provider %c%c", p0, p1);
  if (version == 0) {
    add_line(r, "Version 0, %s. Station not identified.", who);
    set_summary(r, "Squitter (%s)", who);
    return true;
  }
  if (t.n < 12 || !is_upper(t.at(4)) || !is_upper(t.at(5)) || !is_upper(t.at(6))) return false;
  char iata[4], icao[5];
  std::memcpy(iata, t.p + 4, 3);
  iata[3] = '\0';
  std::memcpy(icao, t.p + 7, 4);
  icao[4] = '\0';
  const char number = t.at(11);
  add_line(r, "Ground station %s (%s) number %c, %s, version %u.", iata, icao, number, who, version);
  char where[40] = "";
  size_t pos = 12;
  if (version == 2 && all_digits(t.p + pos, 4) && (t.at(pos + 4) == 'N' || t.at(pos + 4) == 'S') &&
      all_digits(t.p + pos + 5, 5) && (t.at(pos + 10) == 'E' || t.at(pos + 10) == 'W')) {
    // Latitude ddmm + N/S, longitude dddmm + E/W, whole minutes.
    const unsigned latd = dec(t.p + pos, 2), latm = dec(t.p + pos + 2, 2);
    const unsigned lond = dec(t.p + pos + 5, 3), lonm = dec(t.p + pos + 8, 2);
    char lat[16], lon[16];
    fmt_coord(lat, sizeof(lat), t.at(pos + 4), latd * 1000u + (latm * 1000u + 30u) / 60u);
    fmt_coord(lon, sizeof(lon), t.at(pos + 10), lond * 1000u + (lonm * 1000u + 30u) / 60u);
    std::snprintf(where, sizeof(where), "%s %s", lat, lon);
    add_line(r, "Located at %s.", where);
    pos += 11;
  }
  // Optional service availability: flag (V/A/B) + six digit kHz, then optional ",address" lists, "-" between services, "/" ends.
  char services[40] = "";
  while (pos + 7 <= t.n) {
    const char f = t.at(pos);
    if ((f == 'V' || f == 'A' || f == 'B') && all_digits(t.p + pos + 1, 6)) {
      const unsigned khz = dec(t.p + pos + 1, 6);
      const char* kind = f == 'V' ? "VDL2" : f == 'A' ? "VDL2 ATN" : "VDL2 AOA+ATN";
      char one[28];
      std::snprintf(one, sizeof(one), "%s%s %u.%03u", services[0] ? ", " : "", kind, khz / 1000u, khz % 1000u);
      if (std::strlen(services) + std::strlen(one) < sizeof(services)) std::strcat(services, one);
      pos += 7;
      while (pos < t.n && (t.at(pos) == ',' || t.at(pos) == '-')) {
        if (t.at(pos) == '-') { ++pos; break; }
        pos += 8;   // ",52ABCDE": comma plus a 7 character ground station address
      }
    } else {
      break;
    }
  }
  if (services[0]) add_line(r, "Offers %s MHz.", services);
  // Free text after "/".
  while (pos < t.n && t.at(pos) != '/') ++pos;
  if (pos + 1 < t.n) {
    char free_text[40];
    size_t k = 0;
    for (size_t i = pos + 1; i < t.n && k + 1 < sizeof(free_text); ++i) free_text[k++] = t.at(i) >= 0x20 && t.at(i) < 0x7f ? t.at(i) : '.';
    free_text[k] = '\0';
    add_line(r, "Note: %s", free_text);
  }
  set_summary(r, "Squitter %s %s%s%s%s", iata, who, where[0] ? " " : "", where, services[0] ? " VDL" : "");
  if (services[0]) set_summary(r, "Squitter %s %s %s", iata, where, services);
  return true;
}

// FMC position report: "POS" + latitude/longitude as hemisphere + degrees + minutes with one decimal, then comma separated:
// waypoint, time, altitude (hundreds of feet), next waypoint, its ETA, following waypoint, temperature, wind.
bool interpret_fmc_position(const Text& t, size_t at, Interpretation& r) {
  if (!t.has(at, "POS")) return false;
  size_t p = at + 3;
  const char ns = t.at(p), ew = t.at(p + 6);
  if (!((ns == 'N' || ns == 'S') && (ew == 'E' || ew == 'W'))) return false;
  unsigned lat_m = 0, lon_m = 0;
  if (!parse_degmin(t.p + p + 1, 2, &lat_m) || !parse_degmin(t.p + p + 7, 3, &lon_m)) return false;
  p += 13;
  // Split the rest on commas (at most 14 fields).
  struct F { const char* s; size_t n; } f[14];
  size_t nf = 0;
  size_t i = p;
  while (i < t.n && nf < 14) {
    if (t.at(i) != ',') break;
    ++i;
    size_t s = i;
    while (i < t.n && t.at(i) != ',' && t.at(i) != '/') ++i;
    f[nf++] = {t.p + s, i - s};
    if (i < t.n && t.at(i) == '/') break;
  }
  char lat[16], lon[16], alt[16] = "", at_wp[16], hms[12];
  fmt_coord(lat, sizeof(lat), ns, lat_m);
  fmt_coord(lon, sizeof(lon), ew, lon_m);
  set_headline(r, "FMC POSITION REPORT");
  // Fields after the position: 0 waypoint, 1 time, 2 altitude (hundreds of feet), 3 next waypoint, 4 its ETA, 5 the one after,
  // 6 temperature, 7 wind.
  if (nf >= 3 && all_digits(f[2].s, f[2].n) && f[2].n <= 3) fmt_alt(alt, sizeof(alt), dec(f[2].s, f[2].n));
  add_line(r, "Position %s %s%s%s", lat, lon, alt[0] ? "   Altitude " : "", alt);
  char over[40] = "";
  if (nf >= 2 && f[0].n >= 2 && f[0].n <= 5 && f[1].n == 6 && all_digits(f[1].s, 6) && valid_hms(f[1].s)) {
    fmt_hms(hms, sizeof(hms), f[1].s);
    std::snprintf(over, sizeof(over), "Over %.*s at %s", static_cast<int>(f[0].n), f[0].s, hms);
  } else if (nf >= 1 && f[0].n >= 2 && f[0].n <= 5) {
    std::snprintf(over, sizeof(over), "Over %.*s", static_cast<int>(f[0].n), f[0].s);
  }
  if (over[0]) {
    if (nf >= 5 && f[3].n >= 2 && f[3].n <= 5 && f[4].n == 6 && all_digits(f[4].s, 6) && valid_hms(f[4].s)) {
      fmt_hms(at_wp, sizeof(at_wp), f[4].s);
      add_line(r, "%s, next %.*s ETA %s", over, static_cast<int>(f[3].n), f[3].s, at_wp);
    } else {
      add_line(r, "%s", over);
    }
  }
  int temp = 0;
  char tbuf[16] = "", wbuf[28] = "";
  if (nf >= 7 && parse_temp(f[6].s, f[6].n, &temp)) std::snprintf(tbuf, sizeof(tbuf), "%dC", temp);
  if (nf >= 8 && f[7].n >= 5 && f[7].n <= 6 && all_digits(f[7].s, f[7].n)) {
    const unsigned dir = dec(f[7].s, 3), spd = dec(f[7].s + 3, f[7].n - 3);
    if (dir <= 360) std::snprintf(wbuf, sizeof(wbuf), "wind %03u/%u kt", dir, spd);
  }
  if (tbuf[0] || wbuf[0]) add_line(r, "Outside air %s   %s", tbuf[0] ? tbuf : "?", wbuf[0] ? wbuf : "");
  set_summary(r, "%s %s %s %s %s", lat, lon, alt, tbuf, wbuf);
  return true;
}

// Frontier-style label 21 position report: "POSN 42.872W120.685, 102,202236,36975,26701, 50,-59,214330,KLAS".
bool interpret_label21(const Text& t, size_t at, Interpretation& r) {
  if (!t.has(at, "POS")) return false;
  size_t p = at + 3;
  const char ns = t.at(p);
  if (ns != 'N' && ns != 'S') return false;
  ++p;
  while (t.at(p) == ' ') ++p;
  size_t s = p;
  while (is_digit(t.at(p)) || t.at(p) == '.') ++p;
  unsigned lat_m = 0, lon_m = 0;
  bool neg = false;
  if (!parse_decimal(t.p + s, p - s, &lat_m, &neg)) return false;
  const char ew = t.at(p);
  if (ew != 'E' && ew != 'W') return false;
  ++p;
  s = p;
  while (is_digit(t.at(p)) || t.at(p) == '.') ++p;
  if (!parse_decimal(t.p + s, p - s, &lon_m, &neg)) return false;
  struct F { const char* s; size_t n; } f[10];
  size_t nf = 0;
  while (p < t.n && t.at(p) == ',' && nf < 10) {
    ++p;
    while (t.at(p) == ' ') ++p;
    const size_t b = p;
    while (p < t.n && t.at(p) != ',') ++p;
    f[nf++] = {t.p + b, p - b};
  }
  char lat[16], lon[16], alt[16] = "", hms[12], eta[12] = "";
  fmt_coord(lat, sizeof(lat), ns, lat_m);
  fmt_coord(lon, sizeof(lon), ew, lon_m);
  set_headline(r, "POSITION REPORT");
  if (nf >= 3 && all_digits(f[2].s, f[2].n) && f[2].n >= 4 && f[2].n <= 5) fmt_alt_ft(alt, sizeof(alt), dec(f[2].s, f[2].n));
  add_line(r, "Position %s %s%s%s", lat, lon, alt[0] ? "   Altitude " : "", alt);
  if (nf >= 2 && f[1].n == 6 && all_digits(f[1].s, 6) && valid_hms(f[1].s)) {
    fmt_hms(hms, sizeof(hms), f[1].s);
    add_line(r, "Reported at %s", hms);
  }
  int temp = 0;
  char tbuf[16] = "";
  if (nf >= 6 && parse_temp(f[5].s, f[5].n, &temp)) std::snprintf(tbuf, sizeof(tbuf), "%dC", temp);
  const bool have_dest = nf >= 8 && f[7].n == 4 && is_upper(f[7].s[0]) && is_upper(f[7].s[1]) && is_upper(f[7].s[2]) && is_upper(f[7].s[3]);
  if (nf >= 7 && f[6].n == 6 && all_digits(f[6].s, 6) && valid_hms(f[6].s)) fmt_hms(eta, sizeof(eta), f[6].s);
  if (tbuf[0] || eta[0] || have_dest) {
    char line[kInterpLineChars];
    int k = 0;
    if (tbuf[0]) k += std::snprintf(line + k, sizeof(line) - static_cast<size_t>(k), "Outside air %s   ", tbuf);
    if (eta[0] || have_dest)
      k += std::snprintf(line + k, sizeof(line) - static_cast<size_t>(k), "ETA %s%s%.*s", eta[0] ? eta : "?", have_dest ? " at " : "",
                         have_dest ? 4 : 0, have_dest ? f[7].s : "");
    add_line(r, "%s", line);
  }
  set_summary(r, "%s %s %s %s", lat, lon, alt, tbuf);
  return true;
}

// Flight data recorder unit (#DF) text: cruise records "N43.0955.W122.0090.282014.#39999.-59.2.267.056.CR#" (decimal degrees,
// day+time, altitude in feet, temperature, wind), a "TRP#time#lat#lon#level#..." block, and a "/CCN" header with the route.
struct DfPos {
  bool ok = false;
  char ns = 'N', ew = 'W';
  unsigned lat_m = 0, lon_m = 0;
  unsigned day = 0;
  char hm[5]{};
  unsigned alt_ft = 0;
  int temp = 0;
  unsigned wdir = 0, wspd = 0;
  char phase[3]{};
};
bool match_df_record(const Text& t, size_t i, DfPos* out, size_t* end) {
  // N dd.dddd . E/W ddd.dddd . dddddd . # ddddd . -dd.d . ddd . ddd . XX #
  if (!(t.at(i) == 'N' || t.at(i) == 'S')) return false;
  size_t p = i + 1;
  if (!(all_digits(t.p + p, 2) && t.at(p + 2) == '.' && all_digits(t.p + p + 3, 4) && t.at(p + 7) == '.')) return false;
  unsigned lat_m = 0, lon_m = 0;
  bool neg = false;
  if (!parse_decimal(t.p + p, 7, &lat_m, &neg)) return false;
  p += 8;
  if (!(t.at(p) == 'E' || t.at(p) == 'W')) return false;
  const char ew = t.at(p);
  ++p;
  if (!(all_digits(t.p + p, 3) && t.at(p + 3) == '.' && all_digits(t.p + p + 4, 4) && t.at(p + 8) == '.')) return false;
  if (!parse_decimal(t.p + p, 8, &lon_m, &neg)) return false;
  p += 9;
  if (!(all_digits(t.p + p, 6) && t.at(p + 6) == '.' && t.at(p + 7) == '#')) return false;
  const unsigned day = dec(t.p + p, 2), hh = dec(t.p + p + 2, 2), mm = dec(t.p + p + 4, 2);
  if (day < 1 || day > 31 || hh > 23 || mm > 59) return false;
  p += 8;
  if (!all_digits(t.p + p, 5) || t.at(p + 5) != '.') return false;
  const unsigned alt = dec(t.p + p, 5);
  p += 6;
  const size_t ts = p;
  if (t.at(p) == '-') ++p;
  while (is_digit(t.at(p))) ++p;
  if (t.at(p) != '.') return false;
  ++p;
  if (!is_digit(t.at(p))) return false;
  ++p;
  int temp = 0;
  if (!parse_temp(t.p + ts, p - ts, &temp)) return false;
  if (t.at(p) != '.') return false;
  ++p;
  if (!(all_digits(t.p + p, 3) && t.at(p + 3) == '.' && all_digits(t.p + p + 4, 3) && t.at(p + 7) == '.')) return false;
  const unsigned wd = dec(t.p + p, 3), ws = dec(t.p + p + 4, 3);
  if (wd > 360) return false;
  p += 8;
  if (!(is_upper(t.at(p)) && is_upper(t.at(p + 1)) && t.at(p + 2) == '#')) return false;
  out->ok = true;
  out->ns = t.at(i);
  out->ew = ew;
  out->lat_m = lat_m;
  out->lon_m = lon_m;
  out->day = day;
  std::snprintf(out->hm, sizeof(out->hm), "%02u%02u", hh, mm);
  out->alt_ft = alt;
  out->temp = temp;
  out->wdir = wd;
  out->wspd = ws;
  out->phase[0] = t.at(p);
  out->phase[1] = t.at(p + 1);
  out->phase[2] = '\0';
  *end = p + 3;
  return true;
}

const char* phase_name(const char* ph) {
  if (std::strcmp(ph, "CR") == 0) return "cruise";
  if (std::strcmp(ph, "CL") == 0) return "climb";
  if (std::strcmp(ph, "DE") == 0 || std::strcmp(ph, "DS") == 0) return "descent";
  return nullptr;
}

bool interpret_dfdau(const Text& t, size_t at, Interpretation& r) {
  // `at` is the first character of the free text after "#DFB".
  size_t b = at;
  while (t.at(b) == ' ') ++b;
  bool any = false;
  set_headline(r, "FLIGHT DATA UNIT REPORT");
  // TRP block: "TRP#202803#044.6863#-122.3252#400#...".
  if (t.has(b, "TRP#")) {
    size_t p = b + 4;
    if (all_digits(t.p + p, 6) && valid_hms(t.p + p) && t.at(p + 6) == '#') {
      char hms[12];
      fmt_hms(hms, sizeof(hms), t.p + p);
      p += 7;
      size_t s = p;
      while (p < t.n && t.at(p) != '#') ++p;
      unsigned lat_m = 0, lon_m = 0;
      bool lat_neg = false, lon_neg = false;
      if (parse_decimal(t.p + s, p - s, &lat_m, &lat_neg) && t.at(p) == '#') {
        ++p;
        s = p;
        while (p < t.n && t.at(p) != '#') ++p;
        if (parse_decimal(t.p + s, p - s, &lon_m, &lon_neg)) {
          char lat[16], lon[16], alt[16] = "";
          fmt_coord(lat, sizeof(lat), lat_neg ? 'S' : 'N', lat_m);
          fmt_coord(lon, sizeof(lon), lon_neg ? 'W' : 'E', lon_m);
          if (t.at(p) == '#') {
            ++p;
            s = p;
            while (p < t.n && t.at(p) != '#') ++p;
            if (p - s >= 2 && p - s <= 3 && all_digits(t.p + s, p - s)) fmt_alt(alt, sizeof(alt), dec(t.p + s, p - s));
          }
          set_headline(r, "FLIGHT DATA UNIT POSITION");
          add_line(r, "Position %s %s%s%s", lat, lon, alt[0] ? "   Level " : "", alt);
          add_line(r, "Reported at %s", hms);
          set_summary(r, "TRP %s %s %s %s", lat, lon, alt, hms);
          any = true;
        }
      }
    }
  }
  // Route: "/CC<tail>,<date>,<time>,<from>,<to>,..." or ".KOAK.KSEA." in the first block of a report.
  char from[5] = "", to[5] = "";
  for (size_t i = at; i + 9 < t.n && !from[0]; ++i) {
    if (t.has(i, "/CC")) {
      size_t p = i + 3;
      char fld[5][12] = {};
      size_t nf = 0, k = 0;
      while (p < t.n && t.at(p) != '/' && nf < 5) {
        if (t.at(p) == ',') { fld[nf][k] = '\0'; ++nf; k = 0; }
        else if (k + 1 < sizeof(fld[0])) fld[nf][k++] = t.at(p);
        ++p;
      }
      if (nf < 5) { fld[nf][k] = '\0'; ++nf; }
      // tail, date, time, origin, destination
      if (nf >= 5 && std::strlen(fld[3]) == 4 && std::strlen(fld[4]) == 4 && is_upper(fld[3][0]) && is_upper(fld[4][0])) {
        std::memcpy(from, fld[3], 5);
        std::memcpy(to, fld[4], 5);
        if (all_digits(fld[2], 6) && fld[0][0]) add_line(r, "Report header: %s  %s  %.2s:%.2s:%.2sZ", fld[0], fld[1], fld[2], fld[2] + 2, fld[2] + 4);
      }
      break;
    }
    if (t.at(i) == '.' && is_upper(t.at(i + 1)) && is_upper(t.at(i + 2)) && is_upper(t.at(i + 3)) && is_upper(t.at(i + 4)) &&
        t.at(i + 5) == '.' && is_upper(t.at(i + 6)) && is_upper(t.at(i + 7)) && is_upper(t.at(i + 8)) && is_upper(t.at(i + 9))) {
      std::memcpy(from, t.p + i + 1, 4);
      std::memcpy(to, t.p + i + 6, 4);
      from[4] = to[4] = '\0';
    }
  }
  if (from[0]) {
    add_line(r, "Route %s to %s", from, to);
    set_summary(r, "%s to %s", from, to);
    any = true;
  }
  // Cruise records.
  DfPos last;
  unsigned count = 0;
  for (size_t i = at; i < t.n; ++i) {
    DfPos pos;
    size_t end = 0;
    if (match_df_record(t, i, &pos, &end)) {
      last = pos;
      ++count;
      i = end - 1;
    }
  }
  if (count > 0) {
    char lat[16], lon[16], alt[16];
    fmt_coord(lat, sizeof(lat), last.ns, last.lat_m);
    fmt_coord(lon, sizeof(lon), last.ew, last.lon_m);
    fmt_alt_ft(alt, sizeof(alt), last.alt_ft);
    const char* ph = phase_name(last.phase);
    set_headline(r, "FLIGHT DATA UNIT POSITIONS");
    add_line(r, "%u position fix%s; latest %s %s   Altitude %s", count, count == 1 ? "" : "es", lat, lon, alt);
    add_line(r, "At day %u %.2s:%.2s   Outside air %dC   wind %03u/%u kt%s%s", last.day, last.hm, last.hm + 2, last.temp, last.wdir,
             last.wspd, ph ? "   " : "", ph ? ph : "");
    set_summary(r, "%u fix%s %s %s %s %dC", count, count == 1 ? "" : "es", lat, lon, alt, last.temp);
    any = true;
  }
  if (!any) {
    add_line(r, "Data block from the flight data recorder unit. Layout not decoded yet; raw text is shown above.");
    set_summary(r, "Flight data unit block");
  }
  return true;
}

bool interpret_h1(const MessageRecord& m, const Text& t, Interpretation& r) {
  const Header h = parse_header(m, t);
  if (!h.ok || t.at(10) != '#' || t.n < 13) return false;
  const char a = t.at(11), b = t.at(12);
  const char* unit = sublabel_name(a, b);
  const bool reserved_b = t.at(13) == 'B';   // ARINC 620: the character after the sub-label is reserved and coded 'B'
  const size_t body = reserved_b ? 14 : 13;
  bool done = false;
  if (a == 'M' && (b == '1' || b == '2' || b == '3' || b == 'D')) done = interpret_fmc_position(t, body, r);
  else if (a == 'D' && b == 'F') done = interpret_dfdau(t, body, r);
  if (done) {
    // The first line identifies the message, then the decoded lines follow.
    Interpretation tmp = r;
    r.line_count = 0;
    add_header_line(r, h);
    for (size_t i = 0; i < tmp.line_count && r.line_count < kInterpLines; ++i) add_line(r, "%s", tmp.lines[i]);
    if (unit) { /* unit name already in the headline */ }
    return true;
  }
  char head[48];
  if (unit) std::snprintf(head, sizeof(head), "MESSAGE FOR/FROM %s", unit);
  else std::snprintf(head, sizeof(head), "H1 MESSAGE, SUB-LABEL %c%c", a, b);
  for (char* c = head; *c; ++c)
    if (*c >= 'a' && *c <= 'z') *c = static_cast<char>(*c - 32);
  set_headline(r, head);
  add_header_line(r, h);
  add_line(r, "Layout of this %s message is not decoded; raw text is shown above.", unit ? unit : "sub-label");
  set_summary(r, "%s %s", unit ? unit : "H1", h.flight);
  return true;
}

}  // namespace

const char* label_name(const char* label) {
  if (!label || !label[0] || !label[1]) return nullptr;
  for (const auto& r : kLabels)
    if (r.a == label[0] && r.b == label[1]) return r.name;
  if (label[0] == '8' && (is_digit(label[1]) || label[1] == '`')) return "Crew-addressed downlink";
  return nullptr;
}

const char* airline_name(const char* flight) {
  if (!flight || !flight[0] || !flight[1] || !is_digit(flight[2])) return nullptr;
  for (const auto& r : kAirlines)
    if (r.a == flight[0] && r.b == flight[1]) return r.name;
  return nullptr;
}

bool interpret(const MessageRecord& m, Interpretation& r) {
  r = Interpretation{};
  const Text t{m.text, m.has_text ? m.text_length : 0u};
  const char* l = m.label;
  bool ok = false;
  if (l[0] == 'A' && l[1] == 'V' && m.mode == '2') {
    // A VDL2 data frame that is not ACARS, kept as received (acars_receiver). The label is ours, not an ACARS label.
    set_headline(r, "VDL2 DATA FRAME");
    add_line(r, "Verified link-layer frame from address %.7s.", m.tail);
    add_line(r, "ATN, CPDLC and ADS-C content is not decoded; the raw bytes are shown as hex.");
    set_summary(r, "VDL2 data frame %.7s", m.tail);
    r.recognised = true;
    return true;
  }
  if (l[0] == '_' && l[1] == '?') ok = interpret_response(m, t, r);
  else if (l[0] == ':' && l[1] == ';') ok = interpret_autotune(t, r);
  else if (l[0] == 'S' && l[1] == 'Q') ok = interpret_squitter(t, r);
  else if (l[0] == 'Q' && l[1] == '0') {
    set_headline(r, "LINK TEST");
    add_line(r, "Routine radio link test between aircraft and ground.");
    const Header h = parse_header(m, t);
    if (h.ok) add_line(r, "Message %.4s   Flight %s", h.msn, h.flight);
    set_summary(r, "Link test%s%s", h.ok ? " " : "", h.ok ? h.flight : "");
    ok = true;
  } else if (l[0] == '_' && l[1] == 'd') {
    set_headline(r, "LINK PING");
    add_line(r, "Demand-mode ping used to keep or test the radio link.");
    set_summary(r, "Link ping");
    ok = true;
  } else if (l[0] == 'H' && l[1] == '1') ok = interpret_h1(m, t, r);
  else if (l[0] == '2' && l[1] == '1') {
    const Header h = parse_header(m, t);
    if (h.ok && interpret_label21(t, 10, r)) {
      Interpretation tmp = r;
      r.line_count = 0;
      add_header_line(r, h);
      for (size_t i = 0; i < tmp.line_count && r.line_count < kInterpLines; ++i) add_line(r, "%s", tmp.lines[i]);
      ok = true;
    }
  }
  const bool parsed = ok;
  if (!ok) {
    // Known label, no text parser: still say what kind of message it is.
    const char* name = label_name(l);
    if (!name) { r = Interpretation{}; return false; }
    r = Interpretation{};
    std::snprintf(r.headline, sizeof(r.headline), "%s", name);
    for (char* c = r.headline; *c; ++c)
      if (*c >= 'a' && *c <= 'z') *c = static_cast<char>(*c - 32);
    const Header h = parse_header(m, t);
    add_header_line(r, h);
    add_line(r, "Label %s. The text layout is not decoded; raw text is shown above.", l);
    set_summary(r, "%s", name);
    ok = true;
  }
  r.recognised = ok;
  r.decoded = parsed;
  return ok;
}

}  // namespace orcsdr::acars_ui
