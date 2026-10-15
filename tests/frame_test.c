#include "../frame.h"
#include "../runtime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Every cached/band/repeated frame must reconstruct exactly what a fresh full
 * render would show, through steady play, seeking, flips, reopen and fades. */
static uint16_t scene[480*272], fresh[480*272];
static uint32_t recovered[480*272];
static unsigned char visits[480*272];
static TapeVertex buffers[2][TAPE_RECTS*2], cache[2048*2];
static TapeFrame frame = { .surface = { scene, sizeof scene, 480, 0 },
                           .cache = cache, .cache_capacity = 2048 };
static const TapeVertex *last;
static int last_count, peak;

static void check(const TapeState *state, unsigned alpha)
{
    TapeSurface surface = { fresh, sizeof fresh, 480, 0 };
    TapeVertex *out = buffers[last == buffers[0]];
    const TapeVertex *shown;
    int n = tape_frame(&frame, state, alpha, out, TAPE_RECTS), i, x, y;
    if (n == TAPE_FRAME_SAME) { assert(last); shown = last; n = last_count; }
    else { assert(n > 0); shown = last = out; last_count = n; }
    if (n > peak) peak = n;
    assert(tape_render(&surface, state) == 0);
    memset(visits, 0, sizeof visits);
    for (i = 0; i < n; ++i) {
        TapeVertex a = shown[i*2], b = shown[i*2+1];
        assert(a.x >= 0 && a.y >= 0 && b.x <= 480 && b.y <= 272 && a.x < b.x && a.y < b.y);
        assert(a.color == b.color && (a.color >> 24) == alpha);
        for (y = a.y; y < b.y; ++y) for (x = a.x; x < b.x; ++x) {
            recovered[y*480+x] = a.color;
            assert(visits[y*480+x]++ == 0);
        }
    }
    for (i = 0; i < 480*272; ++i) {
        uint32_t c = recovered[i];
        uint16_t packed = ((c & 255) >> 3) | (((c >> 8) & 255) >> 2 << 5) |
                          (((c >> 16) & 255) >> 3 << 11);
        assert(visits[i] == 1 && packed == fresh[i]);
    }
}

int main(void)
{
    TapeClock clock = {0};
    TapeState steady = {0, 1, TAPE_ENTRANCE_MS, 0};
    TapePose a, b;
    unsigned i, t;
    /* Steady frames differ only inside the cached band's bounds. */
    for (t = 0; t < 400; ++t) {
        static uint16_t first[480*272];
        TapeSurface s = { t ? fresh : first, sizeof fresh, 480, 0 };
        steady.elapsed_ms = 60000 + t*2111;
        tape_pose(&steady, &a);
        if (a.flipping || a.side) continue; /* Same side as the first frame. */
        assert(tape_render(&s, &steady) == 0);
        if (t) for (i = 0; i < 480*272; ++i)
            if (fresh[i] != first[i]) assert(i/480 >= TAPE_BAND_Y0 && i/480 < TAPE_BAND_Y1);
    }
    /* A playing session: reopen, steady play, fast-forward across a flip,
     * rewind back, pause, fade and reopen again. */
    for (i = 0; i < 1500; ++i) {
        int rate = i < 600 ? 1 : i < 1000 ? TAPE_SEEK_RATE : i < 1300 ? -TAPE_SEEK_RATE : 1;
        if (i == 400) clock.state.elapsed_ms = TAPE_SIDE_MS - 3000; /* Near a flip. */
        tape_clock_tick_rate(&clock, 33, i < 1400 || i > 1450, i != 1200, rate);
        check(&clock.state, i >= 1420 && i < 1430 ? 128 + i - 1420 : 255);
    }
    tape_pose(&clock.state, &b);
    assert(frame.full_frames && frame.band_frames && frame.same_frames);
    printf("Frame cache: %u full, %u band-only and %u repeated frames reconstruct exactly; peak %d rectangles.\n",
           frame.full_frames, frame.band_frames, frame.same_frames, peak);
    return 0;
}
