#include "player.h"
#include "pcm.h"
#include "timer.h"
#include "display.h"
#include "opl2.h"

#include <string.h>
#include <stdio.h>

/* The floppy ships transcoded .RAW files alongside the .EXE. We keep a
 * fixed list so the keys 1..6 always pick the same clip even if a future
 * floppy gains extra files. Filenames are uppercase 8.3 to match what
 * the Makefile installs. */
static const char *CLIP_FILES[] = {
    "DING.RAW",
    "CHORD.RAW",
    "TADA.RAW",
    "CHIMES.RAW",
    "RINGIN.RAW",
    "RINGOUT.RAW",
};
#define CLIP_COUNT (sizeof(CLIP_FILES) / sizeof(CLIP_FILES[0]))

static int      cur_clip   = 0;
static uint16_t cur_total  = 0;
static char     cur_name[16];

static void load_clip(int idx) {
    if (idx < 0) idx = 0;
    if (idx >= (int)CLIP_COUNT) idx = (int)CLIP_COUNT - 1;
    cur_clip  = idx;
    cur_total = pcm_load(CLIP_FILES[idx]);
    strncpy(cur_name, CLIP_FILES[idx], sizeof(cur_name) - 1);
    cur_name[sizeof(cur_name) - 1] = 0;
    if (cur_total == 0) {
        display_error("could not load clip; is the .RAW on the floppy?");
        return;
    }
    timer_pcm_play(pcm_buf, cur_total);
}

static int pcm_init(const char *arg) {
    int idx = 0;

    display_init();
    opl_pcm_setup(timer_wait_ms);

    /* `arg` may be a full filename (FOO.RAW) the user passed on the DOS
     * cmdline, or NULL for the default. Map an exact filename match
     * against the table; anything unrecognized falls back to clip 0. */
    if (arg) {
        size_t i;
        for (i = 0; i < CLIP_COUNT; i++) {
            if (stricmp(arg, CLIP_FILES[i]) == 0) { idx = (int)i; break; }
        }
    }
    load_clip(idx);
    return 0;
}

static void pcm_tick(uint32_t now_ms) {
    static uint32_t last_redraw = 0;
    uint16_t played;
    (void)now_ms;

    /* The ISR runs at 8 kHz; the screen at ~30 Hz is plenty. Reading
     * pcm_pos without disabling interrupts is OK — a torn read just
     * shows a slightly stale progress percentage for one frame. */
    if (now_ms - last_redraw >= 33) {
        played = timer_pcm_done() ? cur_total : timer_pcm_pos();
        display_status(cur_name, cur_total, played);
        last_redraw = now_ms;
    }
}

static void pcm_on_key(int k) {
    if (k == ' ') {
        timer_pcm_play(pcm_buf, cur_total);   /* replay current */
        return;
    }
    if (k >= '1' && k <= '0' + (int)CLIP_COUNT) {
        load_clip(k - '1');
    }
}

static void pcm_cleanup(void) {
    timer_pcm_stop();
    opl_pcm_silence();
}

const player_t PLAYER_PCM = {
    "pcm",
    pcm_init,
    pcm_tick,
    pcm_on_key,
    pcm_cleanup,
};
