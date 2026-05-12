#ifndef PLAYER_H
#define PLAYER_H

#include <stdint.h>

/* Same vtable shape as adlib-rng so future players (Win95 chimes, a
 * test-tone generator, etc.) can slot in without touching main.c. The
 * PCM player is currently the only implementation. */
typedef struct player {
    const char *name;
    int  (*init)(const char *arg);
    void (*tick)(uint32_t now_ms);
    void (*on_key)(int k);
    void (*cleanup)(void);
} player_t;

extern const player_t PLAYER_PCM;

#endif
