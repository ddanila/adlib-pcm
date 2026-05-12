# adlib-pcm

Play the Windows 3.x system sounds on a real AdLib (OPL2 / Yamaha
YM3812) by amplitude-modulating a sustained carrier tone — the
"DAC trick" Sierra and Pinball Fantasies used to coax PCM out of
hardware that was never meant to produce it. Runs in QEMU against
a vendored MS-DOS 4.0 boot floppy.

Not a serious sound engine — a sibling to
[adlib-rng](https://github.com/ddanila/adlib-rng) that exercises a
completely different corner of the OPL2.

## Requirements

- **`dosbox-staging`** — uses NukedOPL by default, the only emulator
  accurate enough for the TL-modulation PCM trick to produce audible
  output. QEMU's `-device adlib` batches register writes and silences
  the technique entirely, which is why this project doesn't bother
  with QEMU.

```sh
# macOS
brew install dosbox-staging

# Debian / Ubuntu
sudo apt install dosbox-staging
```

Open Watcom v2 is vendored — no external install or network needed
to build. The Windows 3.x WAVs are pulled in via a git submodule.

The Windows 3.x WAVs live in the
[`all-windows-sounds`](https://github.com/MCPlayer2015/all-windows-sounds)
submodule under `sources/`. Clone with submodules:

```sh
git clone --recursive https://github.com/ddanila/adlib-pcm
# or, if already cloned:
git submodule update --init
```

## Run

```sh
make run
```

On a fresh clone this transcodes the six Windows 3.x WAVs to 8 kHz
raw PCM, compiles `ADLIB.EXE`, stages everything into `build/dosbox/`,
and launches DOSBox-Staging with NukedOPL in OPL2-compat mode. The
autoexec runs the player; it loads `DING.RAW` by default and waits
for input.

## Usage

From the DOS prompt (hit `ESC` to drop out of the player, then
`ADLIB` to re-enter):

```
A:> ADLIB              load default clip (DING.RAW)
A:> ADLIB TADA.RAW     load a specific clip
```

**Player keys:** `ESC` quit, `SPACE` replay current clip,
`1`-`6` cycle clips (DING, CHORD, TADA, CHIMES, RINGIN, RINGOUT).

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

Six-bit log dynamic range, recognisable Windows 3.x chimes —
about the quality F-15 Strike Eagle II and Sound Club hit on
real hardware. See [NOTES.md](NOTES.md) for the dead-ends, the
trade-offs, and the rest of the technique.

See [NOTES.md](NOTES.md) for layout, build targets, and the
implementation story.

## License

MIT — see [LICENSE](LICENSE). The `vendor/` and `sources/` trees
carry their own licenses from their upstream projects.
