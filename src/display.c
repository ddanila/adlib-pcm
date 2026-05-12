#include "display.h"

#include <stdio.h>
#include <i86.h>

/* Standard Watcom 16-bit has no clrscr()/gotoxy() in conio.h, so go
 * through BIOS int 10h directly. */
static void bios_set_mode(uint8_t mode) {
    union REGS r;
    r.h.ah = 0x00;
    r.h.al = mode;
    int86(0x10, &r, &r);
}

static void bios_gotoxy(uint8_t col, uint8_t row) {
    union REGS r;
    r.h.ah = 0x02;
    r.h.bh = 0x00;     /* page 0 */
    r.h.dh = row;
    r.h.dl = col;
    int86(0x10, &r, &r);
}

void display_init(void) {
    bios_set_mode(0x03);   /* 80x25 text, clears screen */
    printf("adlib-pcm  --  Windows 3.x sounds through an AdLib (OPL2)\r\n");
    printf("ESC: quit   SPACE: replay   1-6: load next clip\r\n");
}

void display_status(const char *filename, uint16_t total, uint16_t played) {
    int pct = (total > 0) ? (int)(((uint32_t)played * 100UL) / total) : 0;
    bios_gotoxy(0, 4);
    printf("clip   : %-12s              \r\n", filename);
    printf("samples: %5u / %5u   [%3d%%]   \r\n", played, total, pct);
}

void display_error(const char *msg) {
    bios_gotoxy(0, 8);
    printf("ERROR: %s                                  \r\n", msg);
}
