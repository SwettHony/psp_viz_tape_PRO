#ifndef VIZ_TAPE_RUNTIME_H
#define VIZ_TAPE_RUNTIME_H
#include <stdint.h>

#define TAPE_ABI 0x54415005u
#define TAPE_READY 0x52554e05u
/* Private runtime loaded from the kernel plugin's embedded PRX. These names
 * differ from the old separately installed renderer to avoid binding to it. */
#define TAPE_RENDERER_MODULE "viz_tape_runtime"
#define TAPE_RENDERER_LIBRARY "VizTapeRuntime"
/* Per-frame rectangle capacity. Measured peaks: 1,198 on hardware (0.8) and
 * 2,164 across sampled host states; overflow drops one frame, never writes. */
#define TAPE_RECTS 4096
typedef void (*TapeDrawArray)(int, int, int, const void *, const void *);
typedef void (*TapeStateFunction)(int);
typedef void (*TapeBlendFunction)(int, int, int, unsigned, unsigned);
typedef struct {
    uint32_t abi;
    uint32_t user_base, user_size;
    TapeDrawArray draw;
    TapeStateFunction enable, disable;
    TapeBlendFunction blend;
    /* Visualizer playback state: 0 play, 1 pause, 2 fast-forward, 3 rewind
     * (confirmed on hardware), 4 stop. Null when the layout check fails; seeking then winds normally. */
    const volatile uint32_t *play_state;
} TapeBindings;
typedef struct {
    uint32_t abi, frames, audio_frames, max_rectangles, errors;
    uint32_t last_alpha, last_frame_us, max_frame_us;
    uint32_t forward_frames, rewind_frames;
} TapeStatus;
typedef struct {
    uint32_t abi, size, ready;
    TapeBindings api;
    TapeStatus status;
} TapeBridge;

#endif
