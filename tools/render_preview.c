#include "../tape.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    static uint32_t pixels[512*272];
    TapeSurface s = {pixels, sizeof pixels, 512, 3};
    TapeState state = {
        argc > 1 ? (uint32_t)strtoul(argv[1], NULL, 10) : 0, 1,
        argc > 2 ? (uint32_t)strtoul(argv[2], NULL, 10) : TAPE_ENTRANCE_MS, 0
    };
    int x, y;
    if (tape_render(&s, &state)) return 1;
    printf("P6\n480 272\n255\n");
    for (y = 0; y < 272; ++y)
        for (x = 0; x < 480; ++x) {
            uint32_t c = pixels[y*512+x];
            putchar(c & 255); putchar((c>>8) & 255); putchar((c>>16) & 255);
        }
    return ferror(stdout) ? 1 : 0;
}
