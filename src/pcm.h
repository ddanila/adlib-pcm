#ifndef PCM_H
#define PCM_H

#include <stdint.h>

/* At 22050 Hz, the longest Windows 3.x clip (TADA, 1.26 s) needs ~28 KB.
 * Cap at 48 KB so we have headroom for slightly longer future clips
 * while staying well under the 64K data segment limit. */
#define PCM_BUF_MAX 48000

/* Load `path` into the global PCM buffer. Returns sample count on
 * success, 0 on failure. Filename is resolved against the DOS
 * working directory (so "DING.RAW" if running from the staged C:
 * root, which is how scripts/run-dosbox.sh sets things up). */
uint16_t pcm_load(const char *path);

extern uint8_t pcm_buf[PCM_BUF_MAX];

#endif
