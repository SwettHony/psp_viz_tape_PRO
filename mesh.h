#ifndef VIZ_TAPE_MESH_H
#define VIZ_TAPE_MESH_H
#include <stddef.h>
#include <stdint.h>

/* GU_COLOR_8888 | GU_VERTEX_16BIT. One pair forms a GU_SPRITES rectangle. */
typedef struct {
    uint32_t color;
    int16_t x, y, z, padding;
} TapeVertex;

/* Returns rectangle count, or -1 on overflow/invalid input. No heap use. */
int tape_mesh(const uint16_t *pixels, int stride, unsigned alpha,
              TapeVertex *vertices, size_t rectangle_capacity);
/* Rows y0..y1-1 only, with absolute coordinates. */
int tape_mesh_rows(const uint16_t *pixels, int stride, unsigned alpha, int y0, int y1,
                   TapeVertex *vertices, size_t rectangle_capacity);
#endif
