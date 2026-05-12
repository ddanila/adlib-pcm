#include "pcm.h"

#include <stdio.h>

uint8_t pcm_buf[PCM_BUF_MAX];

uint16_t pcm_load(const char *path) {
    FILE *fp;
    size_t n;

    fp = fopen(path, "rb");
    if (!fp) return 0;
    n = fread(pcm_buf, 1, PCM_BUF_MAX, fp);
    fclose(fp);
    return (uint16_t)n;
}
