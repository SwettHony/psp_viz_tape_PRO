#include "../tape.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static void advance(TapeClock *clock, unsigned ticks, int playing)
{
    while (ticks--) tape_clock_tick(clock, 100, playing, 1);
}

int main(void)
{
    TapeClock clock = {0};
    TapePose pose;
    TapeState state = {0, 1, TAPE_ENTRANCE_MS, 0};
    unsigned t, saved;
    int left = (int)TAPE_REEL_MAX, right = (int)TAPE_REEL_MIN;
    tape_clock_tick(&clock, 100, 1, 1);
    assert(clock.state.entrance_ms == 0 && clock.state.elapsed_ms == 0);
    advance(&clock, 10, 1);
    assert(clock.state.entrance_ms == TAPE_ENTRANCE_MS && clock.state.elapsed_ms == 0);
    tape_pose(&clock.state, &pose);
    assert(!pose.loading && !pose.flipping && pose.side == 0);
    assert(pose.left_radius == (int)TAPE_REEL_MAX && pose.right_radius == (int)TAPE_REEL_MIN);
    advance(&clock, 9000, 1);
    assert(clock.state.elapsed_ms == TAPE_SIDE_MS);
    tape_pose(&clock.state, &pose);
    assert(pose.flipping && pose.side == 0);
    assert(pose.left_radius == (int)TAPE_REEL_MIN && pose.right_radius == (int)TAPE_REEL_MAX);
    advance(&clock, 100, 0);
    assert(clock.state.elapsed_ms == TAPE_SIDE_MS); /* Pause also freezes a flip. */
    advance(&clock, TAPE_FLIP_MS / 200, 1);
    tape_pose(&clock.state, &pose);
    assert(pose.side == 1 && pose.flipping && pose.yaw_cos == 0 && pose.yaw_sin == 1024);
    assert(pose.left_radius == (int)TAPE_REEL_MAX && pose.right_radius == (int)TAPE_REEL_MIN);
    advance(&clock, TAPE_FLIP_MS / 200, 1);
    tape_pose(&clock.state, &pose);
    assert(pose.side == 1 && !pose.flipping && pose.yaw_cos == 1024);
    advance(&clock, (TAPE_SIDE_MS + TAPE_FLIP_MS) / 100, 1);
    assert(clock.state.elapsed_ms == 0);
    tape_pose(&clock.state, &pose);
    assert(pose.side == 0 && !pose.flipping);

    advance(&clock, 100, 1);
    saved = clock.state.elapsed_ms;
    tape_clock_tick(&clock, 100, 1, 0);
    advance(&clock, 1, 1); /* Reopening inserts the same, partly played cassette. */
    assert(clock.state.entrance_ms == 0 && clock.state.elapsed_ms == saved);
    advance(&clock, 10, 0);
    assert(clock.state.elapsed_ms == saved && clock.state.entrance_ms == TAPE_ENTRANCE_MS);
    tape_clock_tick(&clock, UINT32_MAX, 1, 1);
    assert(clock.state.elapsed_ms == saved);

    for (t = 0; t <= TAPE_SIDE_MS; t += 1000) {
        state.elapsed_ms = t;
        tape_pose(&state, &pose);
        assert(pose.left_radius <= left && pose.right_radius >= right);
        assert(pose.left_radius >= (int)TAPE_REEL_MIN && pose.right_radius <= (int)TAPE_REEL_MAX);
        left = pose.left_radius; right = pose.right_radius;
    }
    for (t = 0; t <= TAPE_ENTRANCE_MS; ++t) {
        state.elapsed_ms = 0; state.entrance_ms = t;
        tape_pose(&state, &pose);
        assert(pose.y_offset >= -264 && pose.y_offset <= 4 && pose.yaw_cos == 1024);
    }
    {
        int previous_cos = 1024;
        state.entrance_ms = TAPE_ENTRANCE_MS;
        for (t = 0; t < TAPE_FLIP_MS; ++t) {
            state.elapsed_ms = TAPE_SIDE_MS + t;
            tape_pose(&state, &pose);
            assert(pose.yaw_cos <= previous_cos && pose.yaw_cos >= -1024);
            assert(pose.yaw_sin >= 0 && pose.yaw_sin <= 1024);
            assert(pose.pitch_sin >= 0 && pose.pitch_sin <= 160);
            assert(pose.pitch_cos >= 1010 && pose.pitch_cos <= 1024);
            assert(pose.retreat >= 0 && pose.retreat <= 380);
            assert(pose.side == (t >= TAPE_FLIP_MS/2));
            previous_cos = pose.yaw_cos;
        }
    }
    state.elapsed_ms = UINT32_MAX; state.entrance_ms = UINT32_MAX;
    tape_pose(&state, &pose);
    assert(pose.side <= 1 && pose.left_radius >= (int)TAPE_REEL_MIN && pose.right_radius <= (int)TAPE_REEL_MAX);
    /* Fast-forward and rewind: the tape winds 12x, the reels turn only 3x
     * (never far enough per 33 ms frame to alias), and seeking crosses flips. */
    {
        TapeClock seek = {0};
        int previous_phase, previous_flip = 0, turned = 0, steps;
        seek.visible = 1; seek.state.entrance_ms = TAPE_ENTRANCE_MS;
        seek.state.elapsed_ms = 60000;
        tape_clock_tick_rate(&seek, 100, 1, 1, TAPE_SEEK_RATE);
        assert(seek.state.elapsed_ms == 60000 + 100*TAPE_SEEK_RATE);
        tape_clock_tick_rate(&seek, 100, 1, 1, -TAPE_SEEK_RATE);
        assert(seek.state.elapsed_ms == 60000);
        tape_clock_tick_rate(&seek, 100, 0, 1, TAPE_SEEK_RATE);
        assert(seek.state.elapsed_ms == 60000); /* Not playing: no seek. */
        tape_pose(&seek.state, &pose);
        previous_phase = pose.phase;
        for (steps = 0; steps < 3000; ++steps) {
            int delta;
            tape_clock_tick_rate(&seek, 33, 1, 1, TAPE_SEEK_RATE);
            assert(seek.state.spin_offset_ms >= 0 &&
                   seek.state.spin_offset_ms < (int32_t)TAPE_SPIN_PERIOD_MS);
            tape_pose(&seek.state, &pose);
            delta = (previous_phase - pose.phase) & 31; /* Phase falls as tape winds. */
            /* A flip restarts the reel angle, as in normal playback. */
            if (!pose.flipping && !previous_flip && delta) {
                assert(delta <= 2); /* Below half a tooth spacing (5.3 steps). */
                turned += delta;
            }
            previous_phase = pose.phase;
            previous_flip = pose.flipping;
        }
        assert(turned > 0);
        /* 3000 x 33 ms at 12x moved ~20 minutes: past the first flip. */
        tape_pose(&seek.state, &pose);
        assert(pose.side == 1);
        turned = 0;
        for (steps = 0; steps < 3000; ++steps) {
            int delta;
            tape_clock_tick_rate(&seek, 33, 1, 1, -TAPE_SEEK_RATE);
            tape_pose(&seek.state, &pose);
            delta = (pose.phase - previous_phase) & 31; /* Rewind turns back. */
            if (!pose.flipping && !previous_flip && delta) { assert(delta <= 2); turned += delta; }
            previous_phase = pose.phase;
            previous_flip = pose.flipping;
        }
        assert(turned > 0 && seek.state.elapsed_ms == 60000); /* Rewound to the start. */
        tape_pose(&seek.state, &pose);
        assert(pose.side == 0);
        seek.state.elapsed_ms = 1000;
        tape_clock_tick_rate(&seek, 100, 1, 1, -TAPE_SEEK_RATE);
        assert(seek.state.elapsed_ms == TAPE_CYCLE_MS - 200); /* Wraps like auto-reverse. */
    }
    puts("Animation: loading, 15-minute A/B transfer, pause, flip, reopen, seeking and clock bounds passed.");
    return 0;
}
