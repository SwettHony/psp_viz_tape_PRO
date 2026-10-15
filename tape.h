#ifndef VIZ_TAPE_H
#define VIZ_TAPE_H

#include <stddef.h>
#include <stdint.h>

/* PSP display formats: 565, 5551, 4444, ABGR8888. */
typedef struct {
    void *pixels;
    size_t bytes;
    int stride;
    int format;
} TapeSurface;

typedef struct {
    /* Active playback time, including automatic side flips; excludes loading. */
    uint32_t elapsed_ms;
    int playing;
    uint32_t entrance_ms;
    /* Reel angle offset (0-1919 ms of normal winding). Seeking winds the tape
     * faster than the reels visibly spin; zero for normal playback. */
    int32_t spin_offset_ms;
} TapeState;

#define TAPE_REEL_MIN 28u
#define TAPE_REEL_MAX 100u

#define TAPE_SIDE_MS (15u * 60u * 1000u)
#define TAPE_FLIP_MS 2000u
#define TAPE_ENTRANCE_MS 1000u
#define TAPE_CYCLE_MS (2u * (TAPE_SIDE_MS + TAPE_FLIP_MS))
/* Fast-forward/rewind: tape winds 12x, reels spin 3x. Six teeth at ~30 fps
 * read as spinning backwards (wagon-wheel aliasing) above about 4x. */
#define TAPE_SEEK_RATE 12
#define TAPE_SEEK_SPIN 3
#define TAPE_SPIN_PERIOD_MS (32u * 60u)

typedef struct {
    TapeState state;
    int visible;
} TapeClock;

typedef struct {
    int side, left_radius, right_radius, phase;
    int y_offset, loading, flipping;
    /* Q10 rotation, plus a camera retreat in scene pixels. */
    int yaw_cos, yaw_sin, pitch_cos, pitch_sin, retreat;
} TapePose;

/* Delta is capped to 100 ms. Hidden views and paused playback do not wind tape. */
void tape_clock_tick(TapeClock *clock, uint32_t delta_ms, int playing, int visible);
/* Rate 1 is normal playback; +/-TAPE_SEEK_RATE fast-forwards or rewinds. */
void tape_clock_tick_rate(TapeClock *clock, uint32_t delta_ms, int playing,
                          int visible, int rate);
void tape_pose(const TapeState *state, TapePose *pose);

/* Draw a 480x272 scene. Invalid surfaces are rejected before any writes. */
int tape_render(const TapeSurface *surface, const TapeState *state);
/* Same scene, writing only rows y0..y1-1; other rows keep their pixels. */
int tape_render_rows(const TapeSurface *surface, const TapeState *state, int y0, int y1);

#endif
