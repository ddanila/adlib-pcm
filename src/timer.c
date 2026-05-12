#include "timer.h"
#include "opl2.h"

#include <conio.h>
#include <dos.h>
#include <i86.h>

#define PIT_FREQ          1193182UL
#define PIT_CH0_DATA      0x40
#define PIT_MODE_PORT     0x43
#define PIC_CMD           0x20
#define PIC_EOI           0x20

/* Once per BIOS_DIVISOR_RATIO pcm ticks we chain to the original INT 8
 * so the time-of-day clock and other BIOS housekeeping keep advancing.
 * PIT_FREQ / 65536 = ~18.2 Hz original rate; PCM_RATE / 18.2 ≈ ratio
 * of our ticks per original tick. Rounded so cumulative drift stays
 * under a second per minute, which is fine for a demo player. */
#define BIOS_DIVISOR_RATIO ((PCM_RATE + 9) / 18)

/* MS_TICK_RATIO: how many ISR ticks per millisecond. At 16 kHz that's 16. */
#define MS_TICK_RATIO     (PCM_RATE / 1000)

static void (__interrupt __far *old_int8)(void) = 0;

static volatile const uint8_t *pcm_buf = 0;
static volatile uint16_t pcm_len = 0;
static volatile uint16_t pcm_pos = 0;

static volatile uint16_t bios_count = 0;
static volatile uint32_t ms_count   = 0;
static volatile uint16_t ms_sub     = 0;

static void __interrupt __far pcm_isr(void) {
    /* Each byte in pcm_buf is already a precomputed OPL2 TL value
     * (0..63 = 0..47 dB attenuation). The transcoder did the linear->log
     * mapping host-side so the ISR's hot path is just two outp()s. */
    if (pcm_pos < pcm_len) {
        outp(OPL_PORT_REG, OPL_PCM_TL_REG);
        outp(OPL_PORT_DATA, pcm_buf[pcm_pos++]);
    } else if (pcm_len != 0) {
        /* Drop to silence on the trailing edge so we don't sit on whatever
         * the last sample's TL was. */
        outp(OPL_PORT_REG, OPL_PCM_TL_REG);
        outp(OPL_PORT_DATA, 0x3F);
    }

    ms_sub++;
    if (ms_sub >= MS_TICK_RATIO) { ms_sub = 0; ms_count++; }

    bios_count++;
    if (bios_count >= BIOS_DIVISOR_RATIO) {
        bios_count = 0;
        /* _chain_intr jumps to the old handler, which will EOI for us. */
        _chain_intr(old_int8);
    } else {
        outp(PIC_CMD, PIC_EOI);
    }
}

static void pit_program(uint16_t divisor) {
    _disable();
    outp(PIT_MODE_PORT, 0x36);             /* ch0, lobyte+hibyte, mode 3 */
    outp(PIT_CH0_DATA, (uint8_t)(divisor & 0xFF));
    outp(PIT_CH0_DATA, (uint8_t)(divisor >> 8));
    _enable();
}

void timer_install(void) {
    /* Round to the nearest divisor instead of truncating — at 16 kHz that
     * picks 75 (giving 15909 Hz, -0.57%) over 74 (16124 Hz, +0.78%). */
    uint16_t div = (uint16_t)((PIT_FREQ + PCM_RATE / 2) / PCM_RATE);
    old_int8 = _dos_getvect(0x08);
    _dos_setvect(0x08, pcm_isr);
    pit_program(div);
}

void timer_restore(void) {
    pit_program(0);                        /* 0 means 65536 -> native ~18.2 Hz */
    if (old_int8) _dos_setvect(0x08, old_int8);
}

void timer_pcm_play(const uint8_t *buf, uint16_t len) {
    _disable();
    pcm_buf = buf;
    pcm_len = len;
    pcm_pos = 0;
    _enable();
}

void timer_pcm_stop(void) {
    _disable();
    pcm_len = 0;
    pcm_pos = 0;
    _enable();
    opl_pcm_silence();
}

int timer_pcm_done(void) {
    int done;
    _disable();
    done = (pcm_len == 0) || (pcm_pos >= pcm_len);
    _enable();
    return done;
}

uint16_t timer_pcm_pos(void) {
    uint16_t p;
    _disable();
    p = pcm_pos;
    _enable();
    return p;
}

uint32_t timer_ms(void) {
    uint32_t v;
    _disable();
    v = ms_count;
    _enable();
    return v;
}

void timer_wait_ms(uint16_t ms) {
    uint32_t target = timer_ms() + ms;
    while (timer_ms() < target) { /* spin */ }
}
