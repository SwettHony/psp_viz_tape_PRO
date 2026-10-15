#ifndef VIZ_TAPE_FRAME_H
#define VIZ_TAPE_FRAME_H
#include "tape.h"
#include "mesh.h"

/* Rows that change while the cassette plays at rest: reels and tape window.
 * Everything above and below is cached until a flip, fade or reopen. */
#define TAPE_BAND_Y0 120
#define TAPE_BAND_Y1 176
#define TAPE_FRAME_SAME (-2)

typedef struct {
    TapeSurface surface;            /* 480x272 RGB565 scene, kept between frames */
    TapeVertex *cache;              /* Cached rectangles above, then below, the band */
    size_t cache_capacity;          /* In rectangles */
    size_t top, bottom;
    int valid, side, have_key;
    unsigned alpha;
    int phase, left, right;
    unsigned full_frames, band_frames, same_frames;
} TapeFrame;

/* Builds this frame's rectangles into out. Returns the rectangle count,
 * TAPE_FRAME_SAME when the previous output can be drawn again unchanged,
 * or -1 on invalid input/overflow (the next frame is then rebuilt fully). */
int tape_frame(TapeFrame *frame, const TapeState *state, unsigned alpha,
               TapeVertex *out, size_t capacity);
#endif
