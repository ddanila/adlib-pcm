#include <conio.h>

#include "opl2.h"
#include "timer.h"
#include "player.h"

int main(int argc, char **argv) {
    const player_t *player = &PLAYER_PCM;
    const char     *arg    = (argc >= 2) ? argv[1] : 0;
    int             quit   = 0;

    opl_init();
    timer_install();

    if (player->init(arg) != 0) {
        timer_restore();
        opl_reset();
        return 1;
    }

    while (!quit) {
        player->tick(timer_ms());

        if (kbhit()) {
            int k = getch();
            if (k == 27) {
                quit = 1;
            } else if (player->on_key) {
                player->on_key(k);
            }
        }
    }

    player->cleanup();
    opl_reset();
    timer_restore();
    return 0;
}
