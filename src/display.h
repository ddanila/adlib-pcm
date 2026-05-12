#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>

void display_init(void);
void display_status(const char *filename, uint16_t total, uint16_t played);
void display_error(const char *msg);

#endif
