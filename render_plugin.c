#include <pspkernel.h>
#include <pspgu.h>
#include "tape.h"
#include "mesh.h"
#include "frame.h"
#include "runtime.h"
#include "safety.h"

#ifndef TAPE_PREVIEW
PSP_MODULE_INFO("viz_tape_render", PSP_MODULE_USER, 1, 0);
PSP_NO_CREATE_MAIN_THREAD();
PSP_HEAP_SIZE_KB(0);
#endif

/* Data export: the patcher never executes renderer code in kernel mode. */
volatile TapeBridge tapeBridge = {
    .abi = TAPE_ABI, .size = sizeof(TapeBridge), .status = { .abi = TAPE_ABI }
};
#define status tapeBridge.status
static uint16_t pixels[480*272] __attribute__((aligned(64)));
/* PAF owns frame submission/synchronization. Retain three frames of vertices
 * just as the native effect retains its vertex data after draw returns. */
static TapeVertex meshes[3][TAPE_RECTS*2] __attribute__((aligned(64)));
/* Cached rectangles above and below the reel band (see frame.h). */
#define TAPE_CACHE_RECTS 2048
static TapeVertex cache[TAPE_CACHE_RECTS*2];
static TapeFrame frame = {
    .surface = { pixels, sizeof pixels, 480, 0 },
    .cache = cache, .cache_capacity = TAPE_CACHE_RECTS
};
static TapeVertex *last_vertices;
static int last_count;
/* Running average of callback time, x16 fixed point. */
static uint32_t average_us16;
static uint32_t previous_tick, audio_tick;
static uint32_t clock_us_remainder;
static TapeClock clock_state;
static unsigned buffer_index;

void tapeDraw(void *effect, const uint32_t *audio)
{
    TapeBindings api;
    uint32_t now, dt, spent;
    float fade;
    unsigned alpha;
    int n, playing, rate = 1;
    TapeVertex *vertices;
    if (tapeBridge.ready != TAPE_READY) return;
    api = tapeBridge.api;
    if (api.abi != TAPE_ABI || !api.draw || !api.enable || !api.disable || !api.blend ||
        !tape_user_range((uintptr_t)effect, 0x28) ||
        !tape_user_range((uintptr_t)audio, 16)) { ++status.errors; return; }
    now = sceKernelGetSystemTimeLow();
    dt = previous_tick ? now - previous_tick : 0;
    previous_tick = now;
    if (dt > 750000) clock_state.visible = 0;
    if (dt > 100000) dt = 0; /* Do not jump after a hidden view or suspend. */
    if (audio[0] && audio[1] && ((const unsigned char *)audio)[12]) {
        audio_tick = now;
        ++status.audio_frames;
    }
    playing = audio_tick && now - audio_tick < 200000;
    if (api.play_state && tape_user_range((uintptr_t)api.play_state, 4)) {
        uint32_t mode = *api.play_state;
        /* Seeking may silence the visualizer audio; it still winds the tape. */
        if (mode == 2) { rate = TAPE_SEEK_RATE; playing = 1; ++status.forward_frames; }
        else if (mode == 3) { rate = -TAPE_SEEK_RATE; playing = 1; ++status.rewind_frames; }
    }
    fade = *(const float *)((const unsigned char *)effect + 0x24);
    /* Ordered comparison also rejects an uninitialized NaN. */
    if (!(fade > 0.0f)) {
        tape_clock_tick(&clock_state, 0, playing, 0);
        return;
    }
    clock_us_remainder += dt;
    tape_clock_tick_rate(&clock_state, clock_us_remainder / 1000, playing, 1, rate);
    clock_us_remainder %= 1000;
    if (fade > 1.0f) fade = 1.0f;
    alpha = (unsigned)(fade * 255.0f);
    vertices = meshes[buffer_index];
    n = tape_frame(&frame, &clock_state.state, alpha, vertices, TAPE_RECTS);
    if (n == TAPE_FRAME_SAME && last_vertices) {
        /* Nothing moved: the GE reads the previous, unchanged buffer again. */
        vertices = last_vertices;
        n = last_count;
    } else if (n < 0) {
        last_vertices = 0;
        ++status.errors;
        return;
    } else {
        sceKernelDcacheWritebackRange(vertices, n * 2 * sizeof *vertices);
        last_vertices = vertices;
        last_count = n;
        buffer_index = (buffer_index + 1) % 3;
    }
    /* Retain the PAF draw path hardware-tested in the earlier spectrum slot.
     * The 2D vertex flag avoids changing native projection/model matrices. */
    api.disable(GU_LIGHTING);
    api.enable(GU_BLEND);
    api.blend(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    api.disable(GU_TEXTURE_2D);
    api.draw(GU_SPRITES, GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D,
             n * 2, 0, vertices);
    /* Leave the reviewed additive blend configuration for later native draws. */
    api.blend(GU_ADD, GU_SRC_ALPHA, GU_FIX, 0, 0xffffffffu);
    ++status.frames;
    if ((unsigned)n > status.max_rectangles) status.max_rectangles = n;
    status.last_alpha = alpha;
    spent = sceKernelGetSystemTimeLow() - now;
    /* render_us in status.txt: average of about the last 16 frames. */
    average_us16 += spent - average_us16 / 16;
    status.last_frame_us = average_us16 / 16;
    if (spent > status.max_frame_us) status.max_frame_us = spent;
}

#ifndef TAPE_PREVIEW
int module_start(SceSize args, void *argp) { (void)args; (void)argp; return 0; }
/* A patched visualizer may still reference our code. Disable via XMB restart. */
int module_stop(SceSize args, void *argp) { (void)args; (void)argp; return -1; }
#endif
