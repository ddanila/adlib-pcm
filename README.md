# adlib-pcm

A small library + DOS player that coaxes PCM audio out of an AdLib
(OPL2 / Yamaha YM3812) by amplitude-modulating a sustained carrier
tone — the "DAC trick" Sierra and Pinball Fantasies used to make
an FM-only chip play digitised sound. Runs under DOSBox-Staging
with NukedOPL.

The repository ships three things:

- **The library** (`src/opl2.{c,h}`, `src/timer.{c,h}`,
  `src/pcm.{c,h}`) — a phase-locked OPL2 carrier setup plus a
  22 kHz timer ISR that streams precomputed TL-register bytes from
  a sample buffer. Independent of any particular clip set.
- **A generic player** (`src/main.c`, `src/player_pcm.c`,
  `src/display.c`) — scans the current DOS directory for `*.RAW`
  files, lets you cycle them with the number keys, and accepts a
  filename on the command line. Builds to `ADLIB.EXE`.
- **A showcase clip set** — the Windows 3.x system sounds, pulled
  in via the `all-windows-sounds` git submodule and transcoded to
  the player's raw TL format by `scripts/transcode_wav.py`. Drop
  in any other 8-bit unsigned mono WAV and it will work the same
  way; the Windows clips are just convenient and recognisable.

Not a serious sound engine — a sibling to
[adlib-rng](https://github.com/ddanila/adlib-rng) that exercises
a completely different corner of the OPL2.

## Requirements

- **`dosbox-staging`** — ships NukedOPL, a cycle-accurate OPL2
  emulator. Other emulators tend to batch register writes inside
  their audio output stage; for the TL-modulation trick we need
  every write honoured at the chip's internal sample rate.

```sh
# macOS
brew install dosbox-staging

# Debian / Ubuntu
sudo apt install dosbox-staging
```

Open Watcom v2 is vendored — no external install or network needed
to build. The Windows 3.x WAVs are pulled in via a git submodule:

```sh
git clone --recursive https://github.com/ddanila/adlib-pcm
# or, if already cloned:
git submodule update --init
```

## Run

```sh
make run
```

On a fresh clone this transcodes the showcase WAVs to 22050 Hz
log-mapped TL bytes (`assets/*.RAW`), compiles `ADLIB.EXE`, stages
everything into `build/dosbox/`, and launches DOSBox-Staging with
NukedOPL in OPL2-compat mode. The autoexec runs the player; it
loads the first `.RAW` it finds and waits for input.

## Usage

```
C:> ADLIB              load the first *.RAW found in cwd
C:> ADLIB TADA.RAW     load a specific clip by name
```

**Player keys:** `ESC` quit, `SPACE` replay current clip,
`1`..`9` cycle through the first nine `*.RAW` files in the
current directory.

## Bring your own audio

Anything `scripts/transcode_wav.py` can read works as a clip.
It expects PCM WAV input (8-bit unsigned or 16-bit signed, mono
or stereo — stereo is mixed down) and writes one log-mapped TL
byte per sample at 22050 Hz to stdout's destination path:

```sh
python3 scripts/transcode_wav.py path/to/your.wav assets/YOUR.RAW
```

The player picks the file up next time it scans the directory.
8.3-style uppercase filenames are required because DOSBox mounts
the staged directory through real-mode DOS APIs.

## How it works

OPL2 has no PCM channel. To fake one we:

1. Set up channel 0's carrier with MULT=10 and F-number 819 at
   block 7, putting its effective frequency at ≈ 49704 Hz —
   phase-locked to within 12 Hz of the chip's internal 49716 Hz
   DAC. Every DAC tick the phase advances by ~one full cycle, so
   the operator emits a constant DC level. The 12 Hz residual is
   below most speakers' bass cutoff and disappears in the analog
   path.
2. Reprogram the 8254 PIT to fire at the PCM sample rate
   (22050 Hz) and install a custom INT 8 ISR.
3. Each ISR tick writes a precomputed TL byte to the carrier's
   register 0x43. The transcoder builds those bytes host-side by
   mapping the signed PCM sample into the log-scaled TL range, so
   silence sits at TL ≈ 8 (mid amplitude) and ±peaks land at
   TL 0 and TL 63 respectively. The chip output is one-sided but
   a DC bias rides through it; the speaker AC-couples that bias
   away, leaving a full bipolar PCM waveform.
4. Every ~1225 ISR ticks we chain to the original INT 8 so the
   BIOS clock keeps ticking and DOS doesn't lose track of itself.

Six-bit log dynamic range — about the quality F-15 Strike Eagle
II and Sound Club hit on real hardware. See [NOTES.md](NOTES.md)
for the dead-ends, the trade-offs, the rest of the technique, and
the source layout.

## License

MIT — see [LICENSE](LICENSE). The `vendor/` and `sources/` trees
carry their own licenses from their upstream projects.
