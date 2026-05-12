#ifndef PCM_H
#define PCM_H

#include <stdint.h>

/* At 22050 Hz, the longest Windows 3.x clip (TADA, 1.26 s) needs ~28 KB.
 * Cap at 48 KB so we have headroom for slightly longer future clips
 * while staying well under the 64K data segment limit. */
#define PCM_BUF_MAX 48000

/* Load `path` into the global PCM buffer. Returns sample count on
 * success, 0 on failure. Filename must be the host-side path inside
 * the DOS environment (so something like "DING.RAW" if running from
 * the floppy root). */
uint16_t pcm_load(const char *path);

extern uint8_t pcm_buf[PCM_BUF_MAX];

#endif
