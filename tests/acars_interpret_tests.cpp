// Host tests for the ACARS interpreter. The texts are real blocks captured from the Tab5 on 131.550 MHz (2026-10-28), verbatim.
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "acars_interpret.hpp"

using namespace orcsdr::acars_ui;

namespace {

[[noreturn]] void fail(const char* expression, int line) {
  std::fprintf(stderr, "FAIL line=%d check=%s\n", line, expression);
  std::exit(1);
}
#define CHECK(expression) \
  do { if (!(expression)) fail(#expression, __LINE__); } while (false)

MessageRecord make(const char* label, char block_id, char ack, const char* text) {
  MessageRecord m;
  m.seq = 1;
  m.mode = '2';
  m.block_id = block_id;
  m.acknowledgement = ack;
  std::snprintf(m.label, sizeof(m.label), "%s", label);
  m.has_text = text[0] != '\0';
  m.text_length = static_cast<uint16_t>(std::strlen(text));
  std::memcpy(m.text, text, m.text_length);
  return m;
}

bool any_line_contains(const Interpretation& r, const char* needle) {
  for (size_t i = 0; i < r.line_count; ++i)
    if (std::strstr(r.lines[i], needle)) return true;
  return false;
}
void dump(const Interpretation& r) {
  std::printf("  [%s] %s\n", r.headline, r.summary);
  for (size_t i = 0; i < r.line_count; ++i) std::printf("    %s\n", r.lines[i]);
}

void squitter() {
  Interpretation r;
  const MessageRecord m = make("SQ", '?', '?', "02XAEUGKEUG14407N12313WV136975/ARINC");
  CHECK(interpret(m, r));
  dump(r);
  CHECK(std::strcmp(r.headline, "GROUND STATION SQUITTER") == 0);
  CHECK(any_line_contains(r, "EUG (KEUG) number 1, ARINC"));
  CHECK(any_line_contains(r, "44.117N 123.217W"));   // 44 deg 07 min, 123 deg 13 min
  CHECK(any_line_contains(r, "VDL2 136.975"));
  CHECK(any_line_contains(r, "ARINC"));
  CHECK(std::strstr(r.summary, "EUG") != nullptr);
  // The ARINC 620 examples: station list and a second service.
  const MessageRecord e = make("SQ", '?', '?', "02XACIDKCID14153N09143WV136975,52ABCDE,52ABCDF-V136925/FreeText");
  CHECK(interpret(e, r));
  CHECK(any_line_contains(r, "VDL2 136.975, VDL2 136.925"));
  CHECK(any_line_contains(r, "Note: FreeText"));
  const MessageRecord v1 = make("SQ", '?', '?', "01XSCIDKCID2/x");
  CHECK(interpret(v1, r));
  CHECK(any_line_contains(r, "CID (KCID) number 2, SITA"));
  CHECK(!interpret(make("SQ", '?', '?', "zz"), r) || r.recognised);   // garbage never crashes
}

void general_response() {
  Interpretation r;
  // Uplink block "W" confirming downlink block 2; the label is "_<DEL>", stored as "_?".
  MessageRecord m = make("_?", 'W', '2', "");
  CHECK(interpret(m, r));
  dump(r);
  CHECK(std::strcmp(r.headline, "GROUND ACKNOWLEDGEMENT") == 0);
  CHECK(any_line_contains(r, "downlink block 2"));
  m = make("_?", 'B', '3', "S83ANW2766");   // ack carrying a message number and flight
  m.acknowledgement = '4';
  CHECK(interpret(m, r));
  CHECK(any_line_contains(r, "Message S83A   Flight NW2766"));
  CHECK(std::strstr(r.summary, "ACK of block 4") != nullptr);
  m = make("_?", '4', '?', "");   // an NAK shows as '?'
  CHECK(interpret(m, r));
  CHECK(std::strcmp(r.headline, "AIRCRAFT ACKNOWLEDGEMENT") == 0);
  CHECK(any_line_contains(r, "no block named"));
}

void autotune_and_tests() {
  Interpretation r;
  CHECK(interpret(make(":;", 'X', '3', "130025"), r));
  dump(r);
  CHECK(any_line_contains(r, "130.025 MHz"));
  CHECK(interpret(make(":;", 'X', '3', "1300250120"), r));
  CHECK(any_line_contains(r, "For 120 seconds"));
  CHECK(interpret(make(":;", 'X', '3', "13"), r));   // too short to parse: the label is still named, nothing is invented
  CHECK(any_line_contains(r, "not decoded"));
  CHECK(!any_line_contains(r, "MHz"));
  CHECK(interpret(make("Q0", '4', '?', "S82ANW2766"), r));
  CHECK(std::strcmp(r.headline, "LINK TEST") == 0);
  CHECK(interpret(make("_d", '1', '?', ""), r));
}

void fmc_position() {
  Interpretation r;
  // Alaska 536 at 20:27:50Z. Degrees and minutes: 44 39.7 N = 44.662, 122 19.2 W = 122.320.
  MessageRecord m = make("H1", '2', '?',
      "F63AAS0536#M1BPOSN44397W122192,KNGDM,202750,400,WRUSL,203243,BTG,M57,268051,93,439,449K,237K,73493B");
  CHECK(interpret(m, r));
  dump(r);
  CHECK(std::strcmp(r.headline, "FMC POSITION REPORT") == 0);
  CHECK(any_line_contains(r, "Message F63 part A   Flight AS0536 (Alaska)"));
  CHECK(any_line_contains(r, "44.662N 122.320W"));
  CHECK(any_line_contains(r, "FL400"));
  CHECK(any_line_contains(r, "Over KNGDM at 20:27:50Z, next WRUSL ETA 20:32:43Z"));
  CHECK(any_line_contains(r, "-57C"));
  CHECK(any_line_contains(r, "wind 268/51 kt"));
  // Five digit wind and the older layout from another aircraft.
  m = make("H1", '6', '?', "F44ANW2766#M1BPOSN43511W121534,HYJOE,203344,389,JAGWA,212751,BURGL,M62,26253,1348D38");
  CHECK(interpret(m, r));
  CHECK(any_line_contains(r, "43.852N 121.890W"));
  CHECK(any_line_contains(r, "FL389"));
  CHECK(any_line_contains(r, "wind 262/53 kt"));
  CHECK(any_line_contains(r, "-62C"));
  // Invalid minutes are rejected rather than guessed.
  m = make("H1", '6', '?', "F44ANW2766#M1BPOSN43911W121534,HYJOE,203344,389");
  CHECK(interpret(m, r));
  CHECK(std::strcmp(r.headline, "MESSAGE FOR/FROM LEFT FMC") == 0);
}

void label21() {
  Interpretation r;
  MessageRecord m = make("21", '0', '?', "M05AF94668POSN 42.872W120.685, 102,202236,36975,26701,  50,-59,214330,KLAS");
  CHECK(interpret(m, r));
  dump(r);
  CHECK(std::strcmp(r.headline, "POSITION REPORT") == 0);
  CHECK(any_line_contains(r, "Flight F94668 (Frontier)"));
  CHECK(any_line_contains(r, "42.872N 120.685W"));
  CHECK(any_line_contains(r, "FL370"));
  CHECK(any_line_contains(r, "20:22:36Z"));
  CHECK(any_line_contains(r, "Outside air -59C"));
  CHECK(any_line_contains(r, "ETA 21:43:30Z at KLAS"));
}

void dfdau() {
  Interpretation r;
  // First block of a cruise report: route plus two complete fixes and a third cut off at the block end.
  MessageRecord m = make("H1", '0', '?',
      "D59AAS0536#DFB  00#  551#.281026.20085400000536.KOAK.KSEA.CR.3700.20237.8818-#  "
      "N42.3526.W122.0239.282008.#40000.-58.0.266.053.CR#  N42.5984.W122.0193.282010.#40004.-59.2.262.056.CR#  "
      "N42.8461.W122.0141.282012.#40000.-57");
  CHECK(interpret(m, r));
  dump(r);
  CHECK(std::strcmp(r.headline, "FLIGHT DATA UNIT POSITIONS") == 0);
  CHECK(any_line_contains(r, "Route KOAK to KSEA"));
  CHECK(any_line_contains(r, "2 position fixes; latest 42.598N 122.019W"));
  CHECK(any_line_contains(r, "Outside air -59C"));
  CHECK(any_line_contains(r, "wind 262/56 kt"));
  // A continuation block (no header) with three fixes.
  m = make("H1", '1', '?',
      "D59BAS0536#DFB.5.263.050.CR#  N43.0955.W122.0090.282014.#39999.-59.2.267.056.CR#  "
      "N43.3439.W122.0059.282016.#40005.-58.2.262.058.CR#  N43.5853.W122.0591.282018.#40001.-57.7.264.052.CR#");
  CHECK(interpret(m, r));
  CHECK(any_line_contains(r, "3 position fixes; latest 43.585N 122.059W"));
  // The decimal degrees agree with the FMC report 13 seconds before it, to within about 2 nm (see the cross-check note).
  m = make("H1", '3', '?', "D60AAS0536#DFB  TRP#202803#044.6863#-122.3252#400#127.4#451#0001767#0.01748#0050.00#0270.00#");
  CHECK(interpret(m, r));
  dump(r);
  CHECK(std::strcmp(r.headline, "FLIGHT DATA UNIT POSITION") == 0);
  CHECK(any_line_contains(r, "44.686N 122.325W"));
  CHECK(any_line_contains(r, "Level FL400"));
  CHECK(any_line_contains(r, "20:28:03Z"));
  // Report header with the route (Northwest-style layout, multi-block part A).
  m = make("H1", '9', '?',
      "D47ANW2766#DFBA319,037438,1,1,TB000000/REP019,01,01,4/CCN327NB,OCT28,203354,KPDX,KLAX,2766/C0TCF05005120004");
  CHECK(interpret(m, r));
  dump(r);
  CHECK(any_line_contains(r, "Route KPDX to KLAX"));
  CHECK(any_line_contains(r, "N327NB"));
  // Later blocks of such a report are recognised as flight data unit blocks but not decoded.
  m = make("H1", '2', '?', "D47DNW2766#DFB03,07680,00096,718,727,718,0/S41273,4495,004,013,202,35,38195,2/T41263,4515");
  CHECK(interpret(m, r));
  CHECK(any_line_contains(r, "Layout not decoded"));
}

void unknown_and_names() {
  Interpretation r;
  CHECK(!interpret(make("ZZ", '1', '?', "whatever"), r));   // unknown label, nothing invented
  CHECK(!r.recognised);
  CHECK(r.line_count == 0);
  // Known label without a parser still gets its meaning.
  CHECK(interpret(make("QA", '1', '?', "M12AUA1234OUT"), r));
  CHECK(std::strstr(r.headline, "OUT REPORT") != nullptr);
  CHECK(std::strcmp(label_name("_?"), "General response (acknowledgement)") == 0);
  CHECK(label_name("85") != nullptr);
  CHECK(label_name("ZZ") == nullptr);
  CHECK(std::strcmp(airline_name("WN1234"), "Southwest") == 0);
  CHECK(std::strcmp(airline_name("DL0456"), "Delta") == 0);
  CHECK(std::strcmp(airline_name("AA0100"), "American") == 0);
  CHECK(airline_name("NW2766") == nullptr);    // not guessed
  CHECK(airline_name("AS") == nullptr);
  // H1 with an unknown sub-label.
  CHECK(interpret(make("H1", '1', '?', "M01AUA1234#ZZBsome text"), r));
  CHECK(std::strstr(r.headline, "SUB-LABEL ZZ") != nullptr);
  // Hostile input: every length from 0 to the maximum, random bytes, no crash (the CI build adds ASan/UBSan).
  unsigned seed = 12345;
  for (int round = 0; round < 3000; ++round) {
    MessageRecord m;
    static const char* labels[] = {"H1", "SQ", "_?", ":;", "21", "Q0", "QA", "_d", "5Z"};
    std::snprintf(m.label, sizeof(m.label), "%s", labels[round % 9]);
    m.has_text = true;
    m.text_length = static_cast<uint16_t>((round * 7) % (sizeof(m.text) + 1));
    for (size_t i = 0; i < m.text_length; ++i) {
      seed = seed * 1103515245u + 12345u;
      const char* alphabet = "NSEW0123456789.,#/-ABCDEFGHIJKLMNOPQRSTUVWXYZ POSV";
      m.text[i] = (seed >> 16) % 7 == 0 ? static_cast<char>(seed >> 8) : alphabet[(seed >> 8) % std::strlen(alphabet)];
    }
    // Also splice real headers so the deeper parsers are reached.
    if (round % 3 == 0 && m.text_length > 20) std::memcpy(m.text, "D59AAS0536#DFB", 14);
    if (round % 5 == 0 && m.text_length > 20) std::memcpy(m.text, "F63AAS0536#M1BPOSN44397W122192", 30 > m.text_length ? m.text_length : 30);
    if (round % 7 == 0 && m.text_length > 30) std::memcpy(m.text, "C87AOO137C#CFBFDE0633591029ABZ 0631570730", m.text_length < 41 ? m.text_length : 41);
    if (round % 11 == 0 && m.text_length > 30) {
      std::memcpy(m.text, "M10AOO137CP,E,2026-10-29 06:23:43,4140,65924,N 44.0", m.text_length < 51 ? m.text_length : 51);
      m.label[0] = '3';
      m.label[1] = '2';
    }
    interpret(m, r);
    CHECK(r.line_count <= kInterpLines);
  }
}

// Live VDL2 captures from the Tab5 on 136.975 MHz (2026-10-29), verbatim.
void live_vdl2_formats() {
  Interpretation r;
  const MessageRecord pos = make("32", '9', '?',
      "M10AOO137CP,E,2026-10-29 06:23:43,4140,65924,N 44.045 W 122.171,14097,249,045,-13,333,368,585,0635,");
  CHECK(interpret(pos, r) && r.decoded);
  dump(r);
  CHECK(std::strcmp(r.headline, "POSITION REPORT") == 0);
  CHECK(any_line_contains(r, "Flight OO137C (SkyWest)"));
  CHECK(any_line_contains(r, "44.045N 122.171W"));
  CHECK(any_line_contains(r, "2026-10-29 06:23:43Z"));
  CHECK(any_line_contains(r, "4140 65924 14097 249 045 -13 333 368 585 0635"));   // shown as received, not named
  CHECK(std::strstr(r.summary, "44.045N 122.171W 06:23:43Z") != nullptr);
  // A different position and a negative-free sample from the same descent.
  const MessageRecord pos2 = make("32", '4', '?',
      "M15AOO137CP,E,2026-10-29 06:25:43,4080,65864,N 44.102 W 122.399,11581,249,029,-9,274,297,468,0636,");
  CHECK(interpret(pos2, r) && any_line_contains(r, "44.102N 122.399W"));
  // Impossible dates, times or coordinates are not decoded.
  CHECK(!interpret(make("32", '9', '?', "M10AOO137CP,E,2026-13-29 06:23:43,4140,65924,N 44.045 W 122.171,1,2,3,"), r) || !r.decoded);
  CHECK(!interpret(make("32", '9', '?', "M10AOO137CP,E,2026-10-29 25:23:43,4140,65924,N 44.045 W 122.171,1,2,3,"), r) || !r.decoded);
  CHECK(!interpret(make("32", '9', '?', "M10AOO137CP,E,2026-10-29 06:23:43,4140,65924,N 94.045 W 122.171,1,2,3,"), r) || !r.decoded);

  const MessageRecord cf = make("H1", '1', '?',
      "C87AOO137C#CFBFDE0633591029ABZ 06315707301    0    0 06315707308    0    0 06320507310    0    0");
  CHECK(interpret(cf, r) && r.decoded);
  dump(r);
  CHECK(std::strcmp(r.headline, "CENTRAL FAULT DISPLAY EVENTS") == 0);
  CHECK(any_line_contains(r, "Flight OO137C (SkyWest)"));
  CHECK(any_line_contains(r, "report 06:33:59Z, tag 1029ABZ. 3 entries"));
  CHECK(any_line_contains(r, "06:31:57 07301  06:31:57 07308  06:32:05 07310"));
  const MessageRecord cf6 = make("H1", '7', '?',
      "C85AOO137C#CFBFDE0630151029ABZ 06281417294    0    0 06281517402    0    0 06283617310    0    0 06283617301    0    0 06283617308    0    0 06295107402    0    0");
  CHECK(interpret(cf6, r) && r.decoded);
  CHECK(any_line_contains(r, "6 entries"));
  CHECK(any_line_contains(r, "06:28:14 17294  06:28:15 17402  06:28:36 17310  06:28:36 17301  06:28:36 17308"));
  CHECK(any_line_contains(r, "06:29:51 07402"));
  // A damaged entry makes the event list undecoded; the unit is still named.
  const MessageRecord bad = make("H1", '1', '?', "C87AOO137C#CFBFDE0633591029ABZ 0631570730    0    0");
  CHECK(interpret(bad, r) && std::strncmp(r.headline, "MESSAGE FOR/FROM", 16) == 0);   // only the unit is named

  // A VDL2 data frame that is not ACARS, kept as raw hex by the receiver.
  MessageRecord av = make("AV", '?', '\0', "2143658711234567008280AA550102");
  std::snprintf(av.tail, sizeof(av.tail), "%s", "1468D2A");
  CHECK(interpret(av, r) && r.recognised && !r.decoded);
  CHECK(std::strcmp(r.headline, "VDL2 DATA FRAME") == 0);
  CHECK(any_line_contains(r, "1468D2A"));
}

}  // namespace

int main() {
  squitter();
  general_response();
  autotune_and_tests();
  fmc_position();
  label21();
  dfdau();
  unknown_and_names();
  live_vdl2_formats();
  std::printf("acars_interpret_tests passed\n");
  return 0;
}
