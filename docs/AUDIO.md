# Original audio sources

All 16 sound cues were synthesized for this project using Python's standard library. No external recordings, sample packs, music, or downloaded audio are included. The editable synthesis source is `Scripts/synthesize_audio.py`; original PCM sources are in `SourceAssets/Audio/`. The content bootstrap imports each source as `/Game/OrbitBreakout/Audio/S_<Cue>`.

Sources use mono, 44,100 Hz, signed 16-bit PCM. Peaks are deliberately below full scale; ambience has a lower peak for mixing. Runtime volume settings still control playback. Source generation was validated for file count, PCM format, nonzero signal, peak below clipping, and exact SHA-256 preservation when the generator was rerun without replacement. In-game mix quality requires listening during play.

| Cue | Seconds | Bytes | Character |
|---|---:|---:|---|
| Shot | 0.19 | 16,802 | Crisp descending electronic pulse |
| Reload | 1.40 | 123,524 | Two latches, charge rise, completion click |
| DryFire | 0.12 | 10,628 | Restrained empty click |
| Damage | 0.28 | 24,740 | Low impact |
| Pickup | 0.48 | 42,380 | Rising two-note confirmation |
| Charge | 0.45 | 39,734 | Rising attack telegraph |
| EnemyShot | 0.24 | 21,212 | Lower, harsher pulse |
| Impact | 0.13 | 11,510 | Metallic spark tick |
| EnemyDamage | 0.22 | 19,448 | Mechanical crackle |
| EnemyDeath | 0.72 | 63,548 | Descending power-down |
| Door | 0.65 | 57,374 | Servo motion and latch |
| Terminal | 0.80 | 70,604 | Three-note acceptance chime |
| Checkpoint | 0.72 | 63,548 | Subtle two-note checkpoint cue |
| Ambience | 8.00 | 705,644 | Seamless low machinery hum |
| Alarm | 2.00 | 176,444 | Two separated warning tones |
| Launch | 8.00 | 705,644 | Latches and rising engine spool |

`Ambience` contains exactly periodic oscillators and modulation over eight seconds. Enable looping on its imported SoundWave. `Alarm` contains silence between warning groups and can loop deliberately. Other cues should play once. The standard charge lasts 0.45 seconds; a heavy-unit charge can use a 0.5625 playback rate to span 0.8 seconds.

Run the generator with the installed Python 3.11 or newer to create missing sources. Existing files are preserved by default, including manual edits. Supply `--replace` only when intentionally regenerating all source cues. Import changes through the content bootstrap or Unreal's reimport action. No runtime Python dependency is needed.
