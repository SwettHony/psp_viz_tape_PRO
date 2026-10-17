/* Assemble with the PSP toolchain so the blob has the same Allegrex/EABI32
 * object format as the kernel plugin. The loader requires 64-byte alignment. */
#include "build/renderer_buffer.h"
    .section .data.tape_renderer,"aw",@progbits
    .balign 64
    .global tape_renderer_prx
    .type tape_renderer_prx, @object
tape_renderer_prx:
    .incbin "viz_tape_render.prx"
.Lrenderer_end:
    .size tape_renderer_prx, .Lrenderer_end - tape_renderer_prx
    .balign 4
    .global tape_renderer_prx_size
    .type tape_renderer_prx_size, @object
tape_renderer_prx_size:
    .word .Lrenderer_end - tape_renderer_prx
    .size tape_renderer_prx_size, 4
    .global tape_renderer_buffer_size
    .type tape_renderer_buffer_size, @object
tape_renderer_buffer_size:
    .word TAPE_RENDERER_BUFFER_SIZE
    .size tape_renderer_buffer_size, 4
