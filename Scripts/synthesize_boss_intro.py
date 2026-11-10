"""Generate the original, deterministic mechanical Bull entrance cue."""
import array
import math
import random
import sys
import wave
from pathlib import Path

RATE = 44100
DURATION = 2.7
DESTINATION = Path(__file__).resolve().parents[1] / "SourceAssets/Audio/S_BossIntro.wav"


def main():
    if DESTINATION.exists():
        print("Preserved existing cue:", DESTINATION)
        return
    rng = random.Random(90417)
    values = []
    noise = 0.0
    for n in range(round(RATE * DURATION)):
        t = n / RATE
        progress = t / DURATION
        envelope = math.sin(math.pi * min(1.0, t / .28) / 2) ** 2
        envelope *= min(1.0, (DURATION - t) / .55) ** 2
        noise += .035 * (rng.uniform(-1, 1) - noise)
        phase = 2 * math.pi * (88 * t - 22 * t * t / DURATION)
        growl = math.tanh(1.6 * (math.sin(phase) + .35 * math.sin(phase * 2.01)))
        growl *= .75 + .25 * math.sin(2 * math.pi * 17 * t)
        servo = math.sin(2 * math.pi * (570 * t - 155 * t * t / DURATION))
        servo *= math.sin(math.pi * progress) ** 2
        sub = math.sin(2 * math.pi * 43 * t)
        values.append(envelope * (.52 * growl + .21 * sub + .13 * servo + 1.4 * noise))
    peak = max(abs(v) for v in values)
    pcm = array.array("h", (round(v / peak * .65 * 32767) for v in values))
    if sys.byteorder != "little":
        pcm.byteswap()
    DESTINATION.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(DESTINATION), "wb") as target:
        target.setnchannels(1)
        target.setsampwidth(2)
        target.setframerate(RATE)
        target.writeframes(pcm.tobytes())
    print("Created", DESTINATION, "2.7s mono PCM; peak -3.74dBFS; no clipped samples")


if __name__ == "__main__":
    main()
