#include "mesh.h"

typedef struct { int x0, x1, rectangle; uint16_t color; } Span;
/* Render callback is serialized by the native visualizer lock. */
static Span rows[2][480];

static uint32_t expand565(uint16_t c, unsigned alpha)
{
    unsigned r = c & 31, g = (c >> 5) & 63, b = c >> 11;
    return ((r << 3) | (r >> 2)) | (((g << 2) | (g >> 4)) << 8) |
           (((b << 3) | (b >> 2)) << 16) | (alpha << 24);
}

int tape_mesh(const uint16_t *pixels, int stride, unsigned alpha,
              TapeVertex *vertices, size_t capacity)
{
    return tape_mesh_rows(pixels, stride, alpha, 0, 272, vertices, capacity);
}

int tape_mesh_rows(const uint16_t *pixels, int stride, unsigned alpha, int y0, int y1,
                   TapeVertex *vertices, size_t capacity)
{
    int y, count = 0, previous_count = 0;
    if (!pixels || !vertices || stride < 480 || stride > 2048 || alpha > 255 ||
        y0 < 0 || y1 > 272 || y0 >= y1)
        return -1;
    for (y = y0; y < y1; ++y) {
        Span *current = rows[y & 1], *previous = rows[(y & 1) ^ 1];
        const uint16_t *p = pixels + y * stride;
        int x = 0, n = 0, prev = 0;
        while (x < 480) {
            int x0 = x++, index;
            uint16_t c = p[x0];
            while (x < 480 && p[x] == c) ++x;
            while (prev < previous_count && previous[prev].x0 < x0) ++prev;
            if (prev < previous_count && previous[prev].x0 == x0 &&
                previous[prev].x1 == x && previous[prev].color == c) {
                index = previous[prev].rectangle;
                vertices[index*2+1].y = y+1;
            } else {
                TapeVertex a, b;
                if ((size_t)count >= capacity) return -1;
                index = count++;
                a.color = b.color = expand565(c, alpha);
                a.x = x0; a.y = y; a.z = a.padding = 0;
                b.x = x; b.y = y+1; b.z = b.padding = 0;
                vertices[index*2] = a;
                vertices[index*2+1] = b;
            }
            current[n].x0 = x0; current[n].x1 = x;
            current[n].rectangle = index; current[n].color = c;
            ++n;
        }
        previous_count = n;
    }
    return count;
}
