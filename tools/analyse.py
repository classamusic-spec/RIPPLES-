#!/usr/bin/env python3
"""
RIPPLES — offline audio analysis.

    tools/analyse.py render.wav [--fundamental 130.81]

Reports the numbers that tell us whether a patch is actually good rather than
merely non-crashing: level, DC offset, stereo width, spectral centroid (a decent
proxy for "how submerged does this sound"), harmonic decay, and — when a
fundamental is given — an inharmonic-energy figure that exposes aliasing.
"""
import sys, wave, argparse, math
import numpy as np


def read_wav(path):
    with wave.open(path, 'rb') as w:
        n_ch, width, rate, n_frames = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
        raw = w.readframes(n_frames)
    dtype = {1: np.uint8, 2: np.int16, 4: np.int32}[width]
    data = np.frombuffer(raw, dtype=dtype).astype(np.float64)
    if width == 1:
        data = (data - 128.0) / 128.0
    else:
        data /= float(2 ** (8 * width - 1))
    return data.reshape(-1, n_ch), rate


def db(x):
    return -np.inf if x <= 1e-12 else 20.0 * math.log10(x)


def analyse(path, fundamental=None):
    data, rate = read_wav(path)
    n, ch = data.shape
    mono = data.mean(axis=1)

    print(f"file            {path}")
    print(f"format          {rate} Hz, {ch} ch, {n} frames ({n/rate:.2f} s)")

    if not np.all(np.isfinite(data)):
        bad = int((~np.isfinite(data)).sum())
        print(f"  !! NON-FINITE  {bad} NaN/Inf samples — FAIL")
        return False

    peak = float(np.max(np.abs(data)))
    rms = float(np.sqrt(np.mean(mono ** 2)))
    dc = float(np.mean(mono))
    print(f"peak            {peak:.4f}  ({db(peak):+.2f} dBFS)")
    print(f"rms             {rms:.4f}  ({db(rms):+.2f} dBFS)")
    print(f"dc offset       {dc:+.6f}")

    if peak < 1e-5:
        print("  !! SILENT — nothing was rendered")
        return False
    if peak > 1.0:
        print(f"  !! CLIPPING — peak exceeds full scale")

    if ch == 2:
        l, r = data[:, 0], data[:, 1]
        denom = math.sqrt(float(np.sum(l ** 2)) * float(np.sum(r ** 2)))
        corr = float(np.sum(l * r)) / denom if denom > 1e-20 else 1.0
        side = float(np.sqrt(np.mean(((l - r) / 2) ** 2)))
        print(f"l/r correlation {corr:+.3f}   (1=mono, 0=wide, <0 = phase issues)")
        print(f"side rms        {side:.4f}  ({db(side):+.2f} dBFS)")

    # Spectrum over the sustain portion, skipping the attack transient.
    start = min(int(0.15 * rate), n // 4)
    seg = mono[start:]
    if len(seg) >= 4096:
        win_n = min(1 << 15, 1 << int(math.log2(len(seg))))
        seg = seg[:win_n] * np.hanning(win_n)
        spec = np.abs(np.fft.rfft(seg))
        freqs = np.fft.rfftfreq(win_n, 1.0 / rate)
        power = spec ** 2
        total = power.sum()
        if total > 1e-20:
            centroid = float((freqs * power).sum() / total)
            cumulative = np.cumsum(power) / total
            rolloff = float(freqs[np.searchsorted(cumulative, 0.85)])
            print(f"spectral centroid {centroid:8.1f} Hz")
            print(f"85% rolloff       {rolloff:8.1f} Hz")

            if fundamental:
                # Energy that is not near a harmonic of the fundamental is either
                # aliasing or intentional inharmonicity — worth knowing either way.
                tol = fundamental * 0.03
                harmonic_mask = np.zeros_like(freqs, dtype=bool)
                k = 1
                while fundamental * k < rate / 2:
                    harmonic_mask |= np.abs(freqs - fundamental * k) < tol
                    k += 1
                inharm = float(power[~harmonic_mask].sum() / total)
                print(f"fundamental       {fundamental:.2f} Hz")
                print(f"inharmonic energy {inharm*100:6.2f} %   "
                      f"(low = clean/band-limited, high = aliasing or inharmonic design)")
    print()
    return True


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("files", nargs="+")
    ap.add_argument("--fundamental", type=float, default=None)
    a = ap.parse_args()
    ok = all(analyse(f, a.fundamental) for f in a.files)
    sys.exit(0 if ok else 1)
