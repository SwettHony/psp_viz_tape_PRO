#ifndef VIZ_TAPE_RENDERER_PAYLOAD_H
#define VIZ_TAPE_RENDERER_PAYLOAD_H
/* renderer_payload.S embeds the user PRX. A temporary, zero-padded user
 * buffer covers the firmware loader's copy of the full memory image. */
extern unsigned char tape_renderer_prx[];
extern const unsigned int tape_renderer_prx_size;
extern const unsigned int tape_renderer_buffer_size;
#endif
