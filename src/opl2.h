#ifndef OPL2_H
#define OPL2_H

#include <stdint.h>

#define OPL_PORT_REG    0x388
#define OPL_PORT_DATA   0x389

/* Carrier-of-channel-0 TL register, 6-bit attenuation (0=loudest, 63=silent).
 * The PCM player writes here at the sample rate. Exposed because the ISR
 * inlines the write for speed. */
#define OPL_PCM_TL_REG  0x43

void opl_init(void);
void opl_reset(void);
void opl_write(uint16_t reg, uint8_t val);

/* Configure channel 0 as a phase-frozen DC source. We start a sine
 * wave at a low audible frequency, key-on (which resets phase to 0),
 * wait one quarter period (the chip's phase counter reaches sin's
 * peak at π/2), then write fnum=0 so the phase counter stops
 * advancing. The operator now emits a constant near-maximum amplitude
 * value forever — no oscillation, no sidebands, no audible carrier
 * tone. The PCM player then writes the carrier's TL register (0x43)
 * at the sample rate to scale that DC level: linear PCM out, modulo
 * the log->linear conversion baked into the transcoded .RAW files.
 *
 * `wait_ms_cb` is a callback main installs that blocks for the
 * given number of milliseconds; opl_pcm_setup() uses it during the
 * phase-freeze dance because the PIT timer ISR is already running
 * by then and the only available wait primitive is the millisecond
 * counter the ISR maintains. */
void opl_pcm_setup(void (*wait_ms_cb)(uint16_t ms));
void opl_pcm_silence(void);

#endif
