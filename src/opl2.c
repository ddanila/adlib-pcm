#include "opl2.h"
#include <conio.h>
#include <i86.h>

/* OPL2 spec: 3.3 us after register select, 23 us after data write. On
 * modern hosts and inside QEMU these are met trivially. We still issue
 * a handful of port reads for paranoia on real hardware, except inside
 * the PCM ISR which writes directly to the ports without going through
 * opl_write() so the per-sample budget stays small. */
static void io_wait_short(void) {
    int i;
    for (i = 0; i < 6; i++) (void)inp(OPL_PORT_REG);
}
static void io_wait_long(void) {
    int i;
    for (i = 0; i < 36; i++) (void)inp(OPL_PORT_REG);
}

/* The OPL2's selected register is global chip state, so the
 * register-select + data-write pair must be atomic w.r.t. the PCM ISR
 * — otherwise the ISR (which also touches 0x388/0x389 to push TL
 * bytes) can race in between the two outp()s and our data byte ends
 * up landing on the ISR's selected register instead. */
void opl_write(uint16_t reg, uint8_t val) {
    _disable();
    outp(OPL_PORT_REG, (uint8_t)reg);
    io_wait_short();
    outp(OPL_PORT_DATA, val);
    io_wait_long();
    _enable();
}

void opl_reset(void) {
    int r;
    for (r = 0; r < 256; r++) opl_write((uint16_t)r, 0);
    /* Enable waveform-select feature; harmless even if we stick to sine. */
    opl_write(0x01, 0x20);
}

void opl_init(void) {
    opl_reset();
}

/* Channel 0, operator pair: modulator @ offset 0x00, carrier @ offset 0x03.
 *
 * To make the carrier inaudible we phase-lock it to the chip's
 * internal DAC rate (49716 Hz). When the operator's effective output
 * frequency equals the DAC rate, the phase counter advances by
 * exactly one full cycle per DAC sample — every sample sees the same
 * phase value, so the operator emits a constant DC level. Writing
 * the carrier's TL register at the PCM sample rate then linearly
 * scales that DC level: clean amplitude modulation, no carrier whine,
 * no sidebands.
 *
 * The OPL2 frequency formula at block=7 is F = fnum * 49716 / 8192.
 * With the per-operator MULT multiplier, F_carrier = MULT * F. For
 * F_carrier = 49716 we need MULT * fnum = 8192. fnum is 10 bits
 * (max 1023) and MULT goes up to 15x, so we can't hit 8192 exactly:
 *   MULT=10, fnum=819 -> 8190 -> 49704 Hz (12 Hz under DAC rate)
 * The 12 Hz residual is a slow tremolo, well below most speakers'
 * usable response (~50 Hz on built-in laptop speakers) so it
 * disappears in the analog path.
 *
 * `wait_ms_cb` is unused here — kept in the signature because callers
 * may want to add startup synchronisation later. */
void opl_pcm_setup(void (*wait_ms_cb)(uint16_t ms)) {
    (void)wait_ms_cb;

    opl_reset();

    /* Modulator: silenced. TL=63 contributes nothing in FM, leaving the
     * carrier to emit a clean sine of its own frequency. */
    opl_write(0x20, 0x01);   /* mult=1, no AM/vib/EG/KSR */
    opl_write(0x40, 0x3F);   /* TL = 63 -> fully attenuated */
    opl_write(0x60, 0xF0);   /* AR=15 (instant), DR=0 */
    opl_write(0x80, 0x0F);   /* SL=0, RR=15 */
    opl_write(0xE0, 0x00);   /* sine */

    /* Carrier: MULT=10, instant attack, hold at full envelope forever. */
    opl_write(0x23, 0x0A);   /* mult=10, no AM/vib/EG/KSR */
    opl_write(OPL_PCM_TL_REG, 0x00);
    opl_write(0x63, 0xF0);   /* AR=15, DR=0 */
    opl_write(0x83, 0x00);   /* SL=0, RR=0 -> hold at full level forever */
    opl_write(0xE3, 0x00);   /* sine */

    opl_write(0xC0, 0x00);   /* feedback=0, FM connection */

    /* fnum = 819 (0x333), block = 7 -> base 4970 Hz; * MULT=10 ≈ 49704 Hz.
     * key-on (bit 5) is asserted; phase resets to 0 here but each DAC
     * tick advances the phase by ~2π so it returns to ~0 every sample. */
    opl_write(0xA0, 0x33);   /* fnum_lo = 819 & 0xFF */
    opl_write(0xB0, 0x3F);   /* key-on | block=7 | fnum_hi=3 */
}

void opl_pcm_silence(void) {
    opl_write(OPL_PCM_TL_REG, 0x3F);
    opl_write(0xB0, 0x00);   /* key-off */
}
