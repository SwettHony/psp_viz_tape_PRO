#include "../tape.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Winding may change the window, but cannot draw over the casing or metadata. */
static void check_layout(void)
{
    static uint32_t start[480*272], end[480*272];
    TapeSurface surface = {start, sizeof start, 480, 3};
    /* Compare matching hub angles at the beginning/end of the winding. */
    TapeState state = {TAPE_SIDE_MS % (32u*60u), 1, TAPE_ENTRANCE_MS, 0};
    int x, y, changed = 0;
    assert(tape_render(&surface, &state) == 0);
    state.elapsed_ms = TAPE_SIDE_MS;
    surface.pixels = end;
    assert(tape_render(&surface, &state) == 0);
    for (y = 0; y < 272; ++y) for (x = 0; x < 480; ++x) {
        if (y < 94) assert(start[y*480+x] == start[0]);
        if (x < 185 || x >= 295 || y < 120 || y >= 176)
            assert(start[y*480+x] == end[y*480+x]);
        else if (start[y*480+x] != end[y*480+x]) ++changed;
    }
    assert(changed > 1000);
    /* A full-bleed shell reaches both edges and the bottom of the display. */
    assert(start[0] == start[479]);
    assert(start[150*480] == start[150*480+479]);
    assert(start[271*480] == start[271*480+479]);
    assert(start[0] != start[150*480] && start[0] != start[271*480]);
    state.elapsed_ms = TAPE_SIDE_MS + TAPE_FLIP_MS;
    assert(tape_render(&surface, &state) == 0);
    /* The main color band changes from red on A to blue on B. */
    {
        uint32_t a = start[100*480+10], b = end[100*480+10];
        assert((a & 255) > ((a >> 16) & 255));
        assert(((b >> 16) & 255) > (b & 255));
        state.elapsed_ms = TAPE_CYCLE_MS;
        assert(tape_render(&surface, &state) == 0);
        assert(end[100*480+10] == a); /* Side B flips back to red Side A. */
    }
    puts("Layout: native metadata clear, shell and hubs unchanged, winding confined to center glass.");
}

int main(void)
{
    int format, x, y;
    TapeState state = {1234, 1, TAPE_ENTRANCE_MS, 0};
    for (format = 0; format < 4; ++format) {
        size_t bpp = format == 3 ? 4 : 2, bytes = 512*272*bpp;
        unsigned char *pixels = malloc(bytes);
        TapeSurface s = {pixels, bytes, 512, format};
        assert(pixels);
        unsigned t, loading;
        for (loading = 0; loading < 2; ++loading) {
            for (t = 0; t <= TAPE_FLIP_MS; t += 50) {
                state.elapsed_ms = TAPE_SIDE_MS + t;
                state.entrance_ms = loading ? 160 : TAPE_ENTRANCE_MS;
                memset(pixels, 0xA5, bytes);
                assert(tape_render(&s, &state) == 0);
                for (y = 0; y < 272; ++y)
                    for (x = 480*(int)bpp; x < 512*(int)bpp; ++x)
                        assert(pixels[y*512*bpp+x] == 0xA5);
            }
        }
        memset(pixels, 0xA5, bytes);
        s.bytes = bytes-1;
        assert(tape_render(&s, &state) == -1);
        for (size_t i = 0; i < bytes; ++i) assert(pixels[i] == 0xA5);
        s.bytes = bytes; s.stride = 479;
        assert(tape_render(&s, &state) == -1);
        s.stride = 512; s.format = 4;
        assert(tape_render(&s, &state) == -1);
        s.format = format; s.pixels = pixels+1;
        assert(tape_render(&s, &state) == -1);
        free(pixels);
    }
    assert(tape_render(NULL, &state) == -1);
    check_layout();
    puts("Renderer: all four formats, stride padding and invalid surfaces passed.");
    return 0;
}
