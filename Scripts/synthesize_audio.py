#!/usr/bin/env python3
"""Create original, deterministic Orbit Breakout sound effects.

No downloads, sample packs, or third-party Python packages are used.
Existing sources are preserved unless --replace is explicitly supplied.
"""

from __future__ import annotations

import argparse
import array
import json
import math
from pathlib import Path
import random
import sys
import wave

RATE = 44100
TAU = math.tau
PROJECT = Path(__file__).resolve().parents[1]
DEFAULT_OUTPUT = PROJECT / "SourceAssets" / "Audio"

# Cue: duration in seconds, target peak (linear, always below full scale).
CUES = {
    "Shot": (0.19, 0.64),
    "Reload": (1.4, 0.48),
    "DryFire": (0.12, 0.32),
    "Damage": (0.28, 0.56),
    "Pickup": (0.48, 0.48),
    "Charge": (0.45, 0.38),
    "EnemyShot": (0.24, 0.56),
    "Impact": (0.13, 0.44),
    "EnemyDamage": (0.22, 0.45),
    "EnemyDeath": (0.72, 0.54),
    "Door": (0.65, 0.40),
    "Terminal": (0.8, 0.44),
    "Checkpoint": (0.72, 0.42),
    "Ambience": (8.0, 0.22),
    "Alarm": (2.0, 0.38),
    "Launch": (8.0, 0.60),
}


def sine(frequency: float, t: float, phase: float = 0.0) -> float:
    return math.sin(TAU * frequency * t + phase)


def chirp(start: float, end: float, t: float, length: float) -> float:
    return math.sin(TAU * (start * t + (end - start) * t * t / (2 * length)))


def soft_gate(t: float, start: float, end: float, edge: float = 0.015) -> float:
    if not start < t < end:
        return 0.0
    attack = min(1.0, (t - start) / edge)
    release = min(1.0, (end - t) / edge)
    return math.sin(attack * math.pi / 2) ** 2 * math.sin(release * math.pi / 2) ** 2


def note(t: float, start: float, frequency: float, length: float = 0.3) -> float:
    elapsed = t - start
    if not 0 < elapsed < length:
        return 0.0
    env = soft_gate(t, start, start + length, 0.008) * math.exp(-elapsed * 7)
    return env * (sine(frequency, elapsed) + 0.18 * sine(frequency * 2, elapsed))


def render(cue: str, duration: float, peak: float) -> array.array:
    # Each cue has an independent stable noise stream; script order is immaterial.
    rng = random.Random(1009 + sum((i + 1) * ord(c) for i, c in enumerate(cue)))
    values = []
    filtered = 0.0
    for index in range(round(duration * RATE)):
        t = index / RATE
        u = t / duration
        noise = rng.uniform(-1.0, 1.0)
        filtered += 0.13 * (noise - filtered)
        if cue == "Shot":
            value = (chirp(1520, 220, t, duration) + 0.24 * chirp(2600, 600, t, duration)
                     + 0.22 * noise) * math.exp(-u * 5.5)
        elif cue == "EnemyShot":
            value = (chirp(540, 95, t, duration) + 0.30 * chirp(1050, 190, t, duration)
                     + 0.32 * filtered) * math.exp(-u * 4.0)
        elif cue == "Reload":
            value = (0.7 * note(t, 0.018, 180, 0.11) + 0.5 * note(t, 0.18, 260, 0.10)
                     + 0.28 * chirp(110, 600, t, duration) * soft_gate(t, 0.3, 1.22, 0.12)
                     + 0.8 * note(t, 1.22, 880, 0.16))
        elif cue == "DryFire":
            value = (0.55 * noise + 0.45 * sine(1350, t)) * math.exp(-t * 65)
        elif cue == "Damage":
            value = (chirp(130, 45, t, duration) + 0.8 * filtered) * math.exp(-u * 5.0)
        elif cue == "Pickup":
            value = note(t, 0.005, 660, 0.26) + 0.9 * note(t, 0.15, 990, 0.32)
        elif cue == "Charge":
            value = (0.8 * chirp(180, 720, t, duration) + 0.18 * chirp(360, 1440, t, duration)) * (0.15 + u * 0.7)
        elif cue == "Impact":
            value = (0.7 * noise + 0.4 * sine(2100, t) + 0.18 * sine(3397, t)) * math.exp(-u * 9)
        elif cue == "EnemyDamage":
            value = (0.6 * noise + 0.5 * sine(480, t) * sine(57, t)) * math.exp(-u * 5)
        elif cue == "EnemyDeath":
            value = (chirp(780, 65, t, duration) + 0.35 * filtered + 0.18 * chirp(1500, 150, t, duration)) * math.exp(-u * 2.8)
        elif cue == "Door":
            value = (0.5 * filtered + 0.3 * sine(95, t) + 0.13 * sine(191, t)) * math.sin(math.pi * u) ** 0.7
            value += 0.2 * note(t, 0.04, 240, 0.08) + 0.4 * note(t, 0.55, 160, 0.09)
        elif cue == "Terminal":
            value = (note(t, 0.008, 440, 0.3) + note(t, 0.19, 660, 0.35)
                     + 0.65 * note(t, 0.41, 880, 0.38))
        elif cue == "Checkpoint":
            value = note(t, 0.008, 523.251, 0.4) + 0.8 * note(t, 0.25, 783.991, 0.46)
        elif cue == "Ambience":
            # Every oscillator and modulation completes an integer number of
            # cycles in eight seconds, making the PCM end/start seam periodic.
            swell = 0.8 + 0.1 * sine(0.125, t) + 0.07 * sine(0.25, t)
            value = swell * (0.6 * sine(60, t) + 0.23 * sine(90, t)
                             + 0.10 * sine(180, t) + 0.06 * sine(241, t))
        elif cue == "Alarm":
            first = soft_gate(t, 0.08, 0.62, 0.08)
            second = soft_gate(t, 0.78, 1.32, 0.08)
            value = 0.6 * (sine(440, t) * first + sine(330, t) * second)
            value += 0.12 * sine(880, t) * first + 0.12 * sine(660, t) * second
        elif cue == "Launch":
            spool = soft_gate(t, 0.0, 8.0, 0.5) * (0.18 + 0.82 * min(t / 4.5, 1))
            value = spool * (0.6 * filtered + 0.38 * chirp(44, 170, t, duration)
                             + 0.11 * chirp(180, 640, t, duration))
            value += 0.45 * note(t, 0.05, 90, 0.16) + 0.45 * note(t, 0.36, 120, 0.17)
        else:
            raise ValueError(cue)
        # Nonlooped cues receive gentle boundary ramps to prevent clicks.
        if cue != "Ambience":
            value *= soft_gate(t, 0.0, duration, min(0.008, duration / 8))
        values.append(value)

    maximum = max(abs(value) for value in values) or 1.0
    result = array.array("h", (round(value / maximum * peak * 32767) for value in values))
    if sys.byteorder != "little":
        result.byteswap()
    return result


def write_wave(path: Path, data: array.array) -> None:
    temporary = path.with_suffix(".wav.tmp")
    with wave.open(str(temporary), "wb") as output:
        output.setnchannels(1)
        output.setsampwidth(2)
        output.setframerate(RATE)
        output.writeframes(data.tobytes())
    temporary.replace(path)


def inspect_wave(path: Path) -> dict:
    with wave.open(str(path), "rb") as source:
        if source.getnchannels() != 1 or source.getsampwidth() != 2 or source.getframerate() != RATE:
            raise ValueError(f"Unexpected PCM format: {path}")
        frames = source.getnframes()
        samples = array.array("h", source.readframes(frames))
        if sys.byteorder != "little":
            samples.byteswap()
    peak = max(abs(value) for value in samples) / 32767
    rms = math.sqrt(sum(value * value for value in samples) / len(samples)) / 32767
    if not 0 < peak < 0.99:
        raise ValueError(f"Silent or clipping source: {path}")
    return {"name": path.name, "duration": frames / RATE, "bytes": path.stat().st_size,
            "peak": round(peak, 4), "rms": round(rms, 4)}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--replace", action="store_true", help="Explicitly regenerate existing cue sources.")
    arguments = parser.parse_args()
    arguments.output.mkdir(parents=True, exist_ok=True)
    results = []
    for cue, (duration, peak) in CUES.items():
        path = arguments.output / f"S_{cue}.wav"
        existed = path.exists()
        if not existed or arguments.replace:
            write_wave(path, render(cue, duration, peak))
        result = inspect_wave(path)
        result["action"] = "preserved" if existed and not arguments.replace else "created"
        results.append(result)
    print(json.dumps({"directory": str(arguments.output), "sample_rate": RATE,
                      "channels": 1, "bits": 16, "cues": results}, indent=2))


if __name__ == "__main__":
    main()
