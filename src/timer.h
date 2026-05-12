#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

/* PCM sample rate. PIT divisor = 1193182 / PCM_RATE.
 * Keep this in sync with TARGET_RATE in scripts/transcode_wav.py.
 * 22050 Hz matches the native rate of the higher-quality Windows 3.x
 * WAVs (DING, CHORD, TADA, CHIMES) so we don't downsample at all
 * for those — eliminates the aliasing that was driving the graininess. */
#define PCM_RATE 22050

/* Install the PCM playback ISR on INT 8 and reprogram PIT channel 0 to
 * fire at PCM_RATE Hz. The ISR consumes samples from the buffer set by
 * timer_pcm_play() and writes them to OPL_PCM_TL_REG. Also keeps the
 * BIOS time-of-day moving by chaining the original INT 8 every Nth tick. */
void timer_install(void);
void timer_restore(void);

/* Hand the ISR a buffer of `len` 8-bit unsigned PCM samples to play. The
 * buffer is read directly from the ISR so it must stay valid until the
 * playback completes or timer_pcm_stop() is called. Returns immediately;
 * playback runs asynchronously on timer interrupts. */
void     timer_pcm_play(const uint8_t *buf, uint16_t len);
void     timer_pcm_stop(void);
int      timer_pcm_done(void);
uint16_t timer_pcm_pos(void);

/* Monotonic millisecond counter, derived from the same ISR; useful for
 * UI tick scheduling without spinning on the BIOS clock. */
uint32_t timer_ms(void);

/* Block the calling context for `ms` milliseconds using the ISR's
 * millisecond counter. Interrupts must already be enabled when this
 * runs (so the ISR keeps ticking). */
void timer_wait_ms(uint16_t ms);

#endif
