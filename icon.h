#ifndef VIZ_TAPE_ICON_H
#define VIZ_TAPE_ICON_H
#include <stddef.h>
#include <stdint.h>
#include "paths.h"
#define TAPE_MUSIC_TEXT_SIZE 0x42b80u
#define TAPE_MUSIC_ICON_PATH 0x3fea4u
#define TAPE_MUSIC_DATA_SIZE 0x9ccu
#define TAPE_MUSIC_BSS_SIZE 0x14c4u
/* Resource-loader function: prefix, path lui/addiu pair, string call, epilogue. */
#define TAPE_MUSIC_LOADER 0x1b564u
#define TAPE_MUSIC_PATH_LUI 0x1b5b0u
#define TAPE_MUSIC_STRING_CALL 0x1b5b8u
#define TAPE_MUSIC_STRING_CTOR 0x3b8d4u
#define TAPE_MUSIC_EPILOGUE 0x1b640u
/* The stock resource: a RAM signature for the redirect, and the read-only
 * input the plugin patches to create TAPE_ICON_PATH on the card. */
extern const char tape_stock_icon_path[];
int tape_icon_resource_ready(void);
int tape_icon_redirect(void *text, size_t size, uint32_t base);

/* One patch step: kind 1 copies length bytes from offset in the stock file,
 * kind 0 copies them from offset in the patch's own data. */
typedef struct { uint32_t kind, offset, length; } TapePatchOp;
typedef struct {
    const TapePatchOp *ops;
    unsigned count;
    const unsigned char *data;
    uint32_t data_size, source_size, output_size, output_fnv;
} TapePatch;
/* Creates TAPE_ICON_PATH from the stock resource on the PSP. A first pass
 * only reads and checks the result; the card is written only if it matches.
 * Returns 1 when a verified resource is in place. */
int tape_icon_build(void);
int tape_icon_build_from(const TapePatch *patch, const char *source, const char *output);
#endif
