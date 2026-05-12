# adlib-pcm — implementation notes

## What this project is

A 16-bit DOS .EXE that plays PCM (Windows 3.x system sounds) on an
AdLib (OPL2 / Yamaha YM3812). The AdLib was an FM synthesis card —
it has no PCM channel, no DAC port, no DMA, nothing. Coaxing PCM out
of it requires a trick.

Built as a sibling to [adlib-rng](https://github.com/ddanila/adlib-rng):
same vendored Open Watcom v2 toolchain, same OPL2 plumbing, but a
completely different musical corner of the chip.

## The technique, in one paragraph

A single OPL2 channel is configured so its carrier operator runs at a
frequency phase-locked to the chip's internal 49716 Hz DAC: every DAC
tick, the operator's phase advances by very nearly one full cycle, so
every output sample reads the same `sin(phase)` value. The operator
emits a constant DC level. A timer ISR running at the PCM sample rate
then writes the carrier's TL (total level, 6-bit logarithmic
attenuation) register on every tick. That linearly scales the DC, and
the speaker's AC coupling filters out the bias, leaving the original
waveform.

The host-side transcoder does the heavy lifting:

- 8-bit PCM → log-mapped TL bytes (one byte per sample, value 0..63),
  so the ISR's hot path is just two `outp()`s.
- Signed mapping with silence parked at TL ≈ 8 (mid-amplitude), so
  positive and negative excursions of the source both reach the
  speaker through the AC-coupled output. This was the single biggest
  audio-quality win in this project.

## Build targets

| target           | what it does                                           |
|------------------|--------------------------------------------------------|
| `make all`       | compile `build/adlib.exe`                              |
| `make raws`      | transcode WAVs from the submodule to `assets/*.RAW`    |
| `make run`       | stage + launch DOSBox-Staging with NukedOPL            |
| `make clean`     | wipe `build/`                                          |
| `make refresh-watcom` | re-vendor a current Open Watcom v2 snapshot       |

## Source layout

```
src/main.c          program entry; player vtable wiring
src/opl2.{c,h}      OPL2 register driver + phase-locked carrier setup
src/timer.{c,h}     custom 8254/PIT ISR at PCM_RATE, plus ms counter
src/pcm.{c,h}       load .RAW file into static buffer
src/player_pcm.c    the only player; pcm load/replay/clip cycling
src/player.h        vtable shape (one player today, room for more)
src/display.{c,h}   minimal text UI via BIOS int 10h
scripts/transcode_wav.py   WAV -> log-TL .RAW transcoder
scripts/run-dosbox.sh      stage + launch DOSBox-Staging
scripts/dosbox.conf        forces OPL2 emulation
```

## Why DOSBox-Staging and not QEMU

QEMU's `-device adlib` batches OPL2 register writes inside its audio
emulation. With our 22 kHz ISR writing TL on every sample, QEMU
coalesces those writes to a much lower effective rate — the audible
output collapses to a faint smear regardless of what you write.

DOSBox-Staging ships [NukedOPL3](https://github.com/nukeykt/Nuked-OPL3),
a cycle-accurate emulator that processes every register write at the
chip's internal 49716 Hz sample rate. That's the only configuration
the PCM trick is audible under (short of real hardware), so it's the
only run target we wire up. An earlier revision of this repo also
had a `make run-qemu` target that booted an MS-DOS floppy through
QEMU — useful for proving the toolchain works end-to-end, useless for
hearing anything. Removed in the cleanup pass.

## Dead-ends and what we learned

Order of attempts, roughly:

1. **AM modulation with carrier at audible frequency** (block=7,
   fnum=max, MULT=1, ~6 kHz). Predictable: a loud 6 kHz tone with the
   PCM signal smeared into its sidebands. Not what we wanted.

2. **Push the carrier above audible (MULT=4)**. Wrong: AM modulation
   produces audio in the *sidebands* around the carrier, so a 24 kHz
   carrier puts the sidebands at 20-28 kHz too. We just stopped
   hearing anything.

3. **Phase-freeze: key-on at low audible freq, wait ¼ period, write
   fnum=0**. Idea was to stop the oscillator at peak amplitude and
   then modulate. Didn't work in QEMU — turned out QEMU's adlib was
   the bigger problem.

4. **Race condition in `opl_write`**. While debugging (3) we found
   that the PIT ISR (writing OPL register 0x43 + data) was preempting
   `opl_write` between its register-select and data-write outs,
   corrupting whatever register was being configured. The chip's
   selected-register is global state — `opl_write` has to wrap the
   pair in CLI/STI. Real bug, real fix.

5. **DAC phase-lock (MULT=10, fnum=819 ≈ 49704 Hz carrier)**. The
   correct technique. The carrier locks to the chip's DAC rate so
   each DAC tick re-reads the same phase and the operator emits
   constant DC. The 12 Hz residual beat is below speaker bass-cutoff
   and inaudible.

6. **QEMU vs DOSBox-Staging**. Through (1)-(5) the audio in QEMU
   stayed unintelligible regardless of OPL2 setup, while DOSBox
   started producing something audible immediately. That's when we
   stopped fighting QEMU.

7. **Log-mapping the TL writes**. The TL register is logarithmic
   (~0.75 dB per step). Writing linear PCM samples into it
   exponentiates the audio. Moving the linear→log mapping to the
   host-side transcoder (one byte = one precomputed TL value) fixed
   the harshest distortion.

8. **Sample-rate sweep**: 8 kHz → 16 kHz → 22050 Hz. Each step
   helped. 22050 matches the native rate of the 22 kHz Windows 3.x
   WAVs so no resampling at all for those — the rest get naive
   linear-interp upsample from 11025 Hz.

9. **Half-wave rectification → signed mapping**. The CHIMES clip
   sounded markedly worse than the others until we noticed it was
   rich in fast zero crossings (bell harmonics). The previous
   `|s - 128|` magnitude mapping treated every zero crossing as
   silence, which the chip duly produced. Switching to the signed
   mapping with silence at TL ≈ 8 fixed it: the chip output is still
   one-sided, but a DC bias rides through, and the speaker
   AC-couples that away leaving the full bipolar waveform.

10. **TPDF dither in TL space**. Toggled on/off via `DITHER` in the
    transcoder. With dither, quantisation grain becomes broadband
    hiss; without, it stays as correlated stair-steps. We ship with
    dither off — at 22050 Hz, with the signed mapping, the
    un-dithered grain is the cleaner trade.

## Quality ceiling

OPL2 PCM via TL modulation is famously crude — F-15 Strike Eagle II
and Sound Club did this and both sound like 4-bit audio underwater.
The fundamental limits we hit:

- The TL register is 6 bits (64 levels) and logarithmic. Mapping a
  full-scale signal into it gives roughly 4-bit *linear* effective
  resolution near peaks, more near silence. The asymmetry shows up
  as audible "grain" on loud passages.

- The chip's maximum register-write rate is ~37 kHz. Push the ISR
  faster and you start dropping writes on real hardware (DOSBox
  doesn't enforce this).

- TL is one-sided. The signed-mapping + AC-coupling trick recovers
  bipolar output, but every positive peak still uses just a few TL
  steps (range [TL 0, TL 8]) while every negative peak gets a lot
  more (range [TL 8, TL 63]). Inherent asymmetry of dB-scale
  representation.

A dual-channel scheme (channel 0 modulating positive samples,
channel 1 modulating negative samples) would unify the dynamic
range. Out of scope here; a future direction if we revisit.

## Refreshing the vendored bundle

```
make refresh-watcom    # downloads current Open Watcom v2 snapshot
```

After it runs, bump `WATCOM_DIR` in the Makefile to the new dated
directory and delete the old one. See
`scripts/vendor_openwatcom.sh` for the gory bits.
