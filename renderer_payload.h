#ifndef VIZ_TAPE_RENDERER_PAYLOAD_H
#define VIZ_TAPE_RENDERER_PAYLOAD_H
/* renderer_payload.S embeds the user PRX in writable, aligned kernel data.
 * The module manager copies/relocates it into the user memory partition. */
extern unsigned char tape_renderer_prx[];
extern const unsigned int tape_renderer_prx_size;
#endif
