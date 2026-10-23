#!/usr/bin/env python3
"""Is the WWV/WWVH once-per-second time tick audible on this Tab5 right now?

WWV (Fort Collins) and WWVH (Hawaii) send a 5 ms burst of 1000 Hz (WWV) or 1200 Hz (WWVH) at the start of every second on 2.5, 5, 10,
15 and 20 MHz (the 29th and 59th second have no tick). If the tick can be found in the receiver's audio, its position inside the second
measures the Tab5's clock error (plus a fixed audio-chain delay and 5-30 ms of propagation), without Wi-Fi.

  python tools/wwv_probe.py --port COM39 --freqs 5 10 15        capture a 15 s slot on each frequency and analyse it
  python tools/wwv_probe.py --wav capture.wav                    analyse a 12 kS/s mono WAV saved with tab5_ft8.py dump
  python tools/wwv_probe.py --selftest                           check the analysis on synthetic ticks and noise

A capture retunes the FT8 receiver for about a minute per frequency (it uses `FT8 DIAL`, an authenticated bench command) and puts the
band back afterwards. It needs the FT8 decoder running (so the audio tap is on) and a clock that is correct, e.g. just after an NTP sync,
because the tick position is reported against the Tab5's own second boundary. Needs numpy and scipy.

Verdict TICKS means the same position (within 3 ms) was found in at least 60 percent of the seconds and the folded profile stands out by
at least 8 dB; anything less is NO SIGNAL (noise alone gives a few dB and a random position every second).
"""
import argparse
import os
import re
import subprocess
import sys
import tempfile
import wave

import numpy as np
from scipy.signal import butter, sosfiltfilt

FS = 12000
BOX_MS = 5            # the tick is a 5 ms burst: a 5 ms boxcar is its matched filter
MIN_CONSISTENCY = 0.6
MIN_FOLD_DB = 8.0


def load_wav(path):
    with wave.open(path, "rb") as w:
        if w.getframerate() != FS or w.getnchannels() != 1 or w.getsampwidth() != 2:
            raise SystemExit("%s: expected 12 kS/s mono 16-bit" % path)
        return np.frombuffer(w.readframes(w.getnframes()), dtype="<i2").astype(np.float64)


def tick_profile(x, centre_hz, half_hz=100):
    """Energy envelope of the band around `centre_hz` at 1 ms steps, one row per whole second."""
    sos = butter(4, [centre_hz - half_hz, centre_hz + half_hz], btype="band", fs=FS, output="sos")
    y = sosfiltfilt(sos, x)
    power = y * y
    per_ms = FS // 1000
    boxed = np.convolve(power, np.ones(BOX_MS * per_ms) / (BOX_MS * per_ms), mode="same")
    ms = boxed[: (len(boxed) // per_ms) * per_ms].reshape(-1, per_ms).mean(axis=1)
    seconds = len(ms) // 1000
    return ms[: seconds * 1000].reshape(seconds, 1000)


def analyse(x, centre_hz=1000):
    """Folds the tick envelope over one second. Returns the tick position, how far it stands out, and how many seconds agree."""
    rows = tick_profile(x, centre_hz)
    if len(rows) < 5:
        return dict(verdict="NO SIGNAL", note="too little audio", tick_ms=None, fold_db=0.0, consistency=0.0, seconds=len(rows))
    fold = rows.mean(axis=0)
    k = int(np.argmax(fold))
    fold_db = 10 * np.log10(fold[k] / (np.median(fold) + 1e-12))
    per_second = np.argmax(rows, axis=1)
    delta = (per_second - k + 500) % 1000 - 500
    consistency = float(np.mean(np.abs(delta) <= 3))
    found = consistency >= MIN_CONSISTENCY and fold_db >= MIN_FOLD_DB
    start = (k - BOX_MS // 2) % 1000     # the matched filter peaks in the middle of the 5 ms burst: report where it starts
    return dict(verdict="TICKS" if found else "NO SIGNAL", tick_ms=start if found else None, peak_ms=k, fold_db=float(fold_db),
                consistency=consistency, seconds=len(rows))


def describe(label, result):
    where = "tick at %d ms into the second" % result["tick_ms"] if result["tick_ms"] is not None else "no consistent tick"
    return "%s: %s - %s (fold %.1f dB over median, %.0f%% of %d seconds agree)" % (
        label, result["verdict"], where, result["fold_db"], 100 * result["consistency"], result["seconds"])


def synthetic(tick_ms=163, seconds=15, amplitude=1.0, seed=1):
    """White noise plus a 5 ms 1000 Hz burst at `tick_ms` into each second (none in second 29 and 59)."""
    rng = np.random.default_rng(seed)
    x = rng.normal(0, 1.0, seconds * FS)
    t = np.arange(int(0.005 * FS)) / FS
    burst = amplitude * np.sin(2 * np.pi * 1000 * t)
    for s in range(seconds):
        if s % 30 == 29:
            continue
        start = int((s + tick_ms / 1000.0) * FS)
        x[start:start + len(burst)] += burst
    return x


def selftest():
    ok = True
    for name, signal, expect in (("strong ticks", synthetic(amplitude=4.0), "TICKS"), ("weak ticks", synthetic(amplitude=2.0), "TICKS"),
                                 ("noise only", synthetic(amplitude=0.0, seed=7), "NO SIGNAL"),
                                 ("pure noise, other seed", synthetic(amplitude=0.0, seed=11), "NO SIGNAL")):
        r = analyse(signal)
        good = r["verdict"] == expect and (expect != "TICKS" or abs(r["tick_ms"] - 163) <= 2)
        print(("PASS " if good else "FAIL ") + describe(name, r))
        ok = ok and good
    return ok


def run_tool(port, *args, timeout=240):
    tool = os.path.join(os.path.dirname(os.path.abspath(__file__)), "tab5_ft8.py")
    return subprocess.run([sys.executable, tool, "--port", port, *args], capture_output=True, text=True, timeout=timeout).stdout


def capture(port, freqs_mhz, wait_s, out_dir):
    status = run_tool(port, "status")
    band = re.search(r" band=(\S+)", status)
    if not band:
        raise SystemExit("no FT8 status from %s: is the unit connected and the FT8 decoder running (FT8 RUN 1)?" % port)
    results = []
    try:
        for mhz in freqs_mhz:
            hz = int(round(mhz * 1e6))
            reply = run_tool(port, "raw", "FT8 DIAL %d" % hz)
            if "TUNE_OK" not in reply:
                print("%g MHz: the receiver refused the tuning: %s" % (mhz, reply.strip()[-120:]))
                continue
            run_tool(port, "watch", str(wait_s), timeout=wait_s + 60)     # a retune skips the slot in progress: wait for whole ones
            path = os.path.join(out_dir, "wwv_%gmhz.wav" % mhz)
            run_tool(port, "dump", path)
            if not os.path.isfile(path):
                print("%g MHz: no audio captured" % mhz)
                continue
            x = load_wav(path)
            results.append((mhz, analyse(x, 1000), analyse(x, 1200)))
            print(describe("%g MHz WWV 1000 Hz" % mhz, results[-1][1]))
            print(describe("%g MHz WWVH 1200 Hz" % mhz, results[-1][2]))
    finally:
        run_tool(port, "band", band.group(1))     # put the band back
        print("band restored to %s" % band.group(1))
    return results


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port")
    ap.add_argument("--freqs", type=float, nargs="+", default=[5, 10, 15], help="WWV frequencies in MHz (2.5 5 10 15 20)")
    ap.add_argument("--wait", type=int, default=50, help="seconds to listen after each retune before the slot is dumped")
    ap.add_argument("--out", default=tempfile.gettempdir(), help="where to keep the captured WAV files")
    ap.add_argument("--wav", help="analyse this saved capture instead of capturing")
    ap.add_argument("--selftest", action="store_true")
    a = ap.parse_args()
    if a.selftest:
        sys.exit(0 if selftest() else 1)
    if a.wav:
        x = load_wav(a.wav)
        print(describe("WWV 1000 Hz", analyse(x, 1000)))
        print(describe("WWVH 1200 Hz", analyse(x, 1200)))
        return
    if not a.port:
        ap.error("--port, --wav or --selftest is required")
    results = capture(a.port, a.freqs, a.wait, a.out)
    found = [(m, r) for m, r1, r2 in results for r in (r1, r2) if r["verdict"] == "TICKS"]
    print("RESULT: ticks heard on %s" % ", ".join("%g MHz" % m for m, _ in found) if found else "RESULT: no WWV tick heard on any frequency tried")


if __name__ == "__main__":
    main()
