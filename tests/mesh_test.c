#include "../tape.h"
#include "../mesh.h"
#include "../runtime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint16_t pixels[480*272];
static uint32_t recovered[480*272];
static unsigned char visits[480*272];
static TapeVertex vertices[TAPE_RECTS*2];

static int check(TapeState state)
{
    TapeSurface surface = {pixels, sizeof pixels, 480, 0};
    int n, i, x, y;
    assert(tape_render(&surface, &state) == 0);
    n = tape_mesh(pixels, 480, 173, vertices, TAPE_RECTS);
    assert(n > 0 && n <= TAPE_RECTS);
    memset(visits, 0, sizeof visits);
    for (i = 0; i < n; ++i) {
        TapeVertex a = vertices[i*2], b = vertices[i*2+1];
        assert(a.x >= 0 && a.y >= 0 && b.x <= 480 && b.y <= 272);
        assert(a.x < b.x && a.y < b.y && a.color == b.color);
        assert(a.z == 0 && b.z == 0 && (a.color >> 24) == 173);
        for (y = a.y; y < b.y; ++y) for (x = a.x; x < b.x; ++x) {
            recovered[y*480+x] = a.color;
            assert(visits[y*480+x]++ == 0);
        }
    }
    for (i = 0; i < 480*272; ++i) {
        uint32_t c = recovered[i];
        uint16_t packed = ((c & 255) >> 3) | (((c >> 8) & 255) >> 2 << 5) |
                          (((c >> 16) & 255) >> 3 << 11);
        assert(visits[i] == 1 && packed == pixels[i]);
    }
    return n;
}

int main(void)
{
    TapeState state = {0, 1, TAPE_ENTRANCE_MS, 0};
    unsigned side, t, phase;
    int peak = 0, n;
#define CHECK() do { n = check(state); if (n > peak) peak = n; } while (0)
    for (side = 0; side < 2; ++side) {
        for (t = 0; t < TAPE_SIDE_MS; t += 15000) {
            for (phase = 0; phase < 1920; phase += 60) {
                state.elapsed_ms = side * (TAPE_SIDE_MS + TAPE_FLIP_MS) + t + phase;
                CHECK();
            }
        }
        /* Perspective faces, shell edge, shadows and both side transitions. */
        for (t = 0; t <= TAPE_FLIP_MS; t += 3) {
            state.elapsed_ms = side * (TAPE_SIDE_MS + TAPE_FLIP_MS) + TAPE_SIDE_MS + t;
            CHECK();
        }
    }
    for (t = 0; t <= TAPE_ENTRANCE_MS; t += 3) {
        state.elapsed_ms = 0; state.entrance_ms = t;
        CHECK();
    }
    state.elapsed_ms = UINT32_MAX; state.entrance_ms = UINT32_MAX; state.playing = 0;
    CHECK();
    assert(tape_mesh(pixels, 480, 255, vertices, 0) == -1);
    assert(tape_mesh(pixels, 480, 256, vertices, TAPE_RECTS) == -1);
    printf("Mesh: winding, reel phases, both flips and loading reconstruct exactly; sampled peak %d / %d rectangles.\n", peak, TAPE_RECTS);
    return 0;
}
