#!/usr/bin/env python3
"""WAV -> .RAW transcoder for the DOS-side PCM player.

The player works by writing the OPL2's Total Level (TL) register at
the sample rate; TL is a logarithmic attenuator (~0.75 dB per step,
range 0..63 where 0 is loudest). Feeding raw linear PCM into that
register produces a sharply distorted output because the chip
exponentiates everything you write. So instead of shipping linear
8-bit samples, this transcoder ships *precomputed TL values* (one
byte per sample, value 0..63). The ISR just writes those bytes
straight to OPL_PORT_DATA with no math.

Half-wave-style folding is unavoidable: TL is a one-sided
attenuator, so a positive sample and its negated counterpart map to
the same magnitude in the OPL's output. We hear the absolute value
of the signal — Pinball-Fantasies-grade PCM, distorted but
recognisable.
"""

import math
import random
import struct
import sys
import wave

TARGET_RATE = 22050
TL_MAX = 63          # silent
DITHER = False       # set True to add ±1-step TPDF dither in TL space
TL_FLOOR_DB = 47.0   # below this many dB-from-peak we just write TL_MAX
TL_STEP_DB = 0.75    # OPL2 TL step size (very close to 3/4 dB)


def read_wav(path: str) -> tuple[int, list[int]]:
    with wave.open(path, "rb") as w:
        ch = w.getnchannels()
        sr = w.getframerate()
        sw = w.getsampwidth()
        frames = w.readframes(w.getnframes())

    if sw == 1:
        samples = list(frames)
        if ch > 1:
            samples = [
                sum(samples[i + c] for c in range(ch)) // ch
                for i in range(0, len(samples), ch)
            ]
    elif sw == 2:
        n = len(frames) // 2
        signed = struct.unpack("<" + "h" * n, frames)
        if ch > 1:
            signed = [
                sum(signed[i + c] for c in range(ch)) // ch
                for i in range(0, len(signed), ch)
            ]
        samples = [(s >> 8) + 128 for s in signed]
    else:
        raise SystemExit(f"unsupported sample width: {sw} bytes")
    return sr, samples


def resample(samples: list[int], src_rate: int, dst_rate: int) -> list[int]:
    if src_rate == dst_rate:
        return list(samples)
    n_in = len(samples)
    n_out = max(1, (n_in * dst_rate) // src_rate)
    step = src_rate / dst_rate
    out: list[int] = []
    for i in range(n_out):
        x = i * step
        a = int(x)
        b = min(a + 1, n_in - 1)
        f = x - a
        v = int(samples[a] * (1.0 - f) + samples[b] * f + 0.5)
        out.append(max(0, min(255, v)))
    return out


def to_tl(samples: list[int]) -> list[int]:
    """Map unsigned 8-bit samples to OPL2 TL bytes (0..63).

    Earlier versions of this function half-wave-rectified the waveform
    (|s - 128| → TL), which sounded fine on slowly-decaying envelopes
    but mangled anything with fast zero crossings — bell harmonics in
    CHIMES, for instance, lost every other half-cycle to "silence".
    Instead, map the *signed* sample into the [0, 1] amp range with
    silence sitting at the midpoint:

        amp = (s_centered + peak) / (2 * peak)

    Now -peak → amp 0 (TL 63), 0 → amp 0.5 (TL ≈ 8), +peak → amp 1
    (TL 0). The chip's output is always one-sided, but a constant
    DC bias rides through it. The speaker AC-couples that DC away
    and what's left is the original waveform at half-amplitude —
    full bipolar PCM, no rectification artefacts.

    Triangular-PDF dither (sum of two uniform draws, range [-1, +1]
    TL steps) is applied before rounding. With only 64 attenuation
    levels the un-dithered output stairsteps audibly when the
    envelope crosses TL boundaries; dither turns that correlated
    error into broadband noise. Fixed seed for reproducible builds.
    """
    rng = random.Random(0xAD11B)
    centered = [s - 128 for s in samples]
    peak = max(abs(s) for s in centered) or 1
    out: list[int] = []
    for s in centered:
        amp = (s + peak) / (2.0 * peak)
        if amp <= 0.0:
            out.append(TL_MAX)
            continue
        db = -20.0 * math.log10(amp)
        if db >= TL_FLOOR_DB:
            out.append(TL_MAX)
            continue
        tl = db / TL_STEP_DB
        if DITHER:
            tl += rng.random() + rng.random() - 1.0
        out.append(max(0, min(TL_MAX, int(round(tl)))))
    return out


def main() -> None:
    if len(sys.argv) != 3:
        raise SystemExit(f"usage: {sys.argv[0]} <in.wav> <out.raw>")
    src, dst = sys.argv[1], sys.argv[2]
    rate, samples = read_wav(src)
    samples = resample(samples, rate, TARGET_RATE)
    tl_bytes = to_tl(samples)
    with open(dst, "wb") as f:
        f.write(bytes(tl_bytes))
    print(
        f"{src} -> {dst}: {len(tl_bytes)} TL samples @ {TARGET_RATE} Hz "
        f"({len(tl_bytes) / TARGET_RATE:.2f}s)"
    )


if __name__ == "__main__":
    main()
