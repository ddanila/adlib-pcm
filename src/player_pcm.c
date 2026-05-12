#include "player.h"
#include "pcm.h"
#include "timer.h"
#include "display.h"
#include "opl2.h"

#include <string.h>
#include <stdio.h>
#include <dos.h>

/* The player is generic: it does not know or care that the shipped
 * showcase happens to be Windows 3.x sounds. On startup it scans the
 * current DOS working directory for *.RAW files and offers the first
 * MAX_CLIPS of them on number keys 1..9. Pass a specific filename as
 * argv[1] (ADLIB FOO.RAW) to load it directly even if it isn't in
 * the scan list. */
#define MAX_CLIPS 9

static char     clips[MAX_CLIPS][16];
static uint8_t  n_clips    = 0;
static int      cur_idx    = 0;
static uint16_t cur_total  = 0;
static char     cur_name[16];

static void scan_clips(void) {
    struct find_t f;
    unsigned rc = _dos_findfirst("*.RAW", _A_NORMAL, &f);
    n_clips = 0;
    while (rc == 0 && n_clips < MAX_CLIPS) {
        strncpy(clips[n_clips], f.name, sizeof(clips[0]) - 1);
        clips[n_clips][sizeof(clips[0]) - 1] = 0;
        n_clips++;
        rc = _dos_findnext(&f);
    }
}

static void load_by_name(const char *name) {
    cur_total = pcm_load(name);
    strncpy(cur_name, name, sizeof(cur_name) - 1);
    cur_name[sizeof(cur_name) - 1] = 0;
    if (cur_total == 0) {
        display_error("could not load clip; is the .RAW on C:?");
        return;
    }
    timer_pcm_play(pcm_buf, cur_total);
}

static void load_clip(int idx) {
    if (n_clips == 0) {
        display_error("no .RAW clips found in current directory");
        return;
    }
    if (idx < 0) idx = 0;
    if (idx >= (int)n_clips) idx = (int)n_clips - 1;
    cur_idx = idx;
    load_by_name(clips[idx]);
}

static int pcm_init(const char *arg) {
    display_init();
    opl_pcm_setup(timer_wait_ms);
    scan_clips();

    if (arg && *arg) {
        /* Explicit filename always wins, even if the scan didn't list it. */
        load_by_name(arg);
        /* Sync cur_idx to it if present in the scan, so number-key cycling
         * picks up from a sensible position. */
        {
            uint8_t i;
            for (i = 0; i < n_clips; i++) {
                if (stricmp(arg, clips[i]) == 0) { cur_idx = i; break; }
            }
        }
    } else if (n_clips > 0) {
        load_clip(0);
    } else {
        display_error("no clip given and no *.RAW files found");
    }
    return 0;
}

static void pcm_tick(uint32_t now_ms) {
    static uint32_t last_redraw = 0;
    uint16_t played;

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
    if (k >= '1' && k <= '0' + (int)n_clips) {
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
