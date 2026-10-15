#include "frame.h"

static void copy(TapeVertex *to, const TapeVertex *from, size_t rectangles)
{
    size_t i;
    for (i = 0; i < rectangles*2; ++i) to[i] = from[i];
}

static int fail(TapeFrame *f)
{
    f->valid = f->have_key = 0;
    return -1;
}

int tape_frame(TapeFrame *f, const TapeState *state, unsigned alpha,
               TapeVertex *out, size_t capacity)
{
    TapePose pose;
    const uint16_t *pixels;
    int steady, top, mid, bottom;
    if (!f || !state || !out || !f->surface.pixels || f->surface.format != 0 ||
        alpha > 255) return -1;
    pixels = f->surface.pixels;
    tape_pose(state, &pose);
    steady = !pose.flipping && !pose.loading;
    if (steady && f->valid && f->side == pose.side && f->alpha == alpha) {
        if (f->have_key && pose.phase == f->phase &&
            pose.left_radius == f->left && pose.right_radius == f->right) {
            ++f->same_frames;
            return TAPE_FRAME_SAME;
        }
        /* Only the reel band changes: redraw it between the cached edges. */
        if (f->top + f->bottom > capacity ||
            tape_render_rows(&f->surface, state, TAPE_BAND_Y0, TAPE_BAND_Y1) < 0)
            return fail(f);
        copy(out, f->cache, f->top);
        mid = tape_mesh_rows(pixels, f->surface.stride, alpha, TAPE_BAND_Y0, TAPE_BAND_Y1,
                             out + f->top*2, capacity - f->top - f->bottom);
        if (mid < 0) return fail(f);
        copy(out + (f->top + mid)*2, f->cache + f->top*2, f->bottom);
        top = (int)f->top; bottom = (int)f->bottom;
        ++f->band_frames;
    } else {
        f->valid = 0;
        if (tape_render(&f->surface, state) < 0) return fail(f);
        top = tape_mesh_rows(pixels, f->surface.stride, alpha, 0, TAPE_BAND_Y0, out, capacity);
        if (top < 0) return fail(f);
        mid = tape_mesh_rows(pixels, f->surface.stride, alpha, TAPE_BAND_Y0, TAPE_BAND_Y1,
                             out + top*2, capacity - top);
        if (mid < 0) return fail(f);
        bottom = tape_mesh_rows(pixels, f->surface.stride, alpha, TAPE_BAND_Y1, 272,
                                out + (top + mid)*2, capacity - top - mid);
        if (bottom < 0) return fail(f);
        if (steady && f->cache && (size_t)(top + bottom) <= f->cache_capacity) {
            copy(f->cache, out, top);
            copy(f->cache + top*2, out + (top + mid)*2, bottom);
            f->top = top; f->bottom = bottom;
            f->valid = 1; f->side = pose.side; f->alpha = alpha;
        }
        ++f->full_frames;
    }
    f->have_key = steady;
    f->phase = pose.phase; f->left = pose.left_radius; f->right = pose.right_radius;
    return top + mid + bottom;
}
