#include "tape.h"

/* Tape transferred on the current side; full and empty reels swap mid-flip. */
static uint32_t winding_of(uint32_t elapsed, int *flipping)
{
    uint32_t local = elapsed % TAPE_CYCLE_MS % (TAPE_SIDE_MS + TAPE_FLIP_MS);
    *flipping = local >= TAPE_SIDE_MS;
    if (!*flipping) return local;
    return local - TAPE_SIDE_MS >= TAPE_FLIP_MS / 2 ? 0 : TAPE_SIDE_MS;
}

void tape_clock_tick(TapeClock *clock, uint32_t delta_ms, int playing, int visible)
{
    tape_clock_tick_rate(clock, delta_ms, playing, visible, 1);
}

void tape_clock_tick_rate(TapeClock *clock, uint32_t delta_ms, int playing,
                          int visible, int rate)
{
    uint32_t remaining, before, after, step;
    int flip_before, flip_after;
    int32_t spin;
    if (!clock) return;
    clock->state.playing = !!playing;
    if (!visible) { clock->visible = 0; return; }
    if (!clock->visible) {
        clock->visible = 1;
        clock->state.entrance_ms = 0;
        delta_ms = 0;
    }
    if (delta_ms > 100) delta_ms = 0;
    if (clock->state.entrance_ms < TAPE_ENTRANCE_MS) {
        remaining = TAPE_ENTRANCE_MS - clock->state.entrance_ms;
        if (delta_ms < remaining) {
            clock->state.entrance_ms += delta_ms;
            return;
        }
        clock->state.entrance_ms = TAPE_ENTRANCE_MS;
        delta_ms -= remaining;
    }
    if (!playing) return;
    if (rate == 1) {
        clock->state.elapsed_ms = (clock->state.elapsed_ms % TAPE_CYCLE_MS + delta_ms) % TAPE_CYCLE_MS;
        return;
    }
    if (rate > TAPE_SEEK_RATE) rate = TAPE_SEEK_RATE;
    if (rate < -TAPE_SEEK_RATE) rate = -TAPE_SEEK_RATE;
    step = delta_ms * (uint32_t)(rate < 0 ? -rate : rate) % TAPE_CYCLE_MS;
    before = winding_of(clock->state.elapsed_ms, &flip_before);
    clock->state.elapsed_ms = rate > 0 ?
        (clock->state.elapsed_ms % TAPE_CYCLE_MS + step) % TAPE_CYCLE_MS :
        (clock->state.elapsed_ms % TAPE_CYCLE_MS + TAPE_CYCLE_MS - step) % TAPE_CYCLE_MS;
    after = winding_of(clock->state.elapsed_ms, &flip_after);
    /* Within one side, cancel the extra winding so the reels turn at the
     * slower seek spin. Flips keep their usual stationary reels. */
    if (flip_before || flip_after || (rate > 0 ? after < before : after > before)) return;
    spin = (int32_t)delta_ms * (rate > 0 ? TAPE_SEEK_SPIN : -TAPE_SEEK_SPIN);
    spin -= (int32_t)after - (int32_t)before;
    spin = (clock->state.spin_offset_ms + spin % (int32_t)TAPE_SPIN_PERIOD_MS) %
           (int32_t)TAPE_SPIN_PERIOD_MS;
    clock->state.spin_offset_ms = spin < 0 ? spin + (int32_t)TAPE_SPIN_PERIOD_MS : spin;
}

static int radius(unsigned area)
{
    int r = TAPE_REEL_MIN;
    while (r < (int)TAPE_REEL_MAX && (unsigned)((r+1)*(r+1)) <= area) ++r;
    return r;
}

/* A quarter-wave table with interpolation keeps the animation integer-only. */
static const unsigned short quarter_sine[65] = {
    0,25,50,75,100,125,150,175,200,224,249,273,297,321,345,369,
    392,415,438,460,483,505,526,548,569,590,610,630,650,669,688,706,
    724,742,759,776,792,808,823,838,851,865,878,891,903,915,926,936,
    946,955,964,972,980,987,993,999,1004,1009,1013,1016,1019,1021,1023,1024,1024
};

/* Phase 0..1024 spans a half-turn. */
static int half_sine(unsigned phase)
{
    unsigned index, part;
    if (phase > 512) phase = 1024 - phase;
    index = phase / 8; part = phase % 8;
    if (index == 64) return 1024;
    return quarter_sine[index] +
           (int)((quarter_sine[index+1] - quarter_sine[index]) * part + 4) / 8;
}

void tape_pose(const TapeState *state, TapePose *pose)
{
    uint32_t time = state->elapsed_ms % TAPE_CYCLE_MS;
    uint32_t local = time % (TAPE_SIDE_MS + TAPE_FLIP_MS);
    uint32_t winding = local > TAPE_SIDE_MS ? TAPE_SIDE_MS : local;
    unsigned area;
    pose->side = time / (TAPE_SIDE_MS + TAPE_FLIP_MS);
    pose->y_offset = 0;
    pose->yaw_cos = pose->pitch_cos = 1024;
    pose->yaw_sin = pose->pitch_sin = pose->retreat = 0;
    pose->loading = state->entrance_ms < TAPE_ENTRANCE_MS;
    pose->flipping = local >= TAPE_SIDE_MS;
    if (pose->flipping) {
        uint32_t flip = local - TAPE_SIDE_MS;
        unsigned progress = flip * 1024 / TAPE_FLIP_MS;
        unsigned angle = (progress * progress * (3072 - 2*progress) + 524288) / 1048576;
        pose->yaw_sin = half_sine(angle);
        pose->yaw_cos = angle <= 512 ? half_sine(512-angle) : -half_sine(angle-512);
        pose->pitch_sin = pose->yaw_sin * 160 / 1024;
        pose->pitch_cos = 1024 - pose->pitch_sin * pose->pitch_sin / 2048;
        pose->retreat = pose->yaw_sin * 380 / 1024;
        pose->y_offset = -pose->yaw_sin * pose->yaw_sin * 24 / 1048576;
        if (flip >= TAPE_FLIP_MS / 2) {
            pose->side ^= 1;
            winding = 0; /* Turning the cassette swaps the full and empty reels. */
        }
    }
    /* Tape length follows annulus area, rather than radius, at constant speed. */
    /* Full-size packs sit behind the shell; their area product exceeds 32 bits. */
    area = (unsigned)((uint64_t)winding *
           (TAPE_REEL_MAX*TAPE_REEL_MAX - TAPE_REEL_MIN*TAPE_REEL_MIN) / TAPE_SIDE_MS);
    pose->left_radius = radius(TAPE_REEL_MAX*TAPE_REEL_MAX - area);
    pose->right_radius = radius(TAPE_REEL_MIN*TAPE_REEL_MIN + area);
    pose->phase = (int)((0u - (winding + (uint32_t)state->spin_offset_ms) / 60) & 31);
    if (pose->loading) {
        unsigned t = state->entrance_ms;
        if (t < 600) {
            int remaining = 600 - (int)t;
            pose->y_offset -= 264 * remaining * remaining / (600 * 600);
        } else if (t < 680) {
            pose->y_offset += (int)(t - 600) * 4 / 80;
        } else if (t < 800) {
            pose->y_offset += (int)(800 - t) * 4 / 120;
        }
    }
}
