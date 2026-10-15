#ifndef VIZ_TAPE_ICON_H
#define VIZ_TAPE_ICON_H
#include <stddef.h>
#include <stdint.h>
#define TAPE_ICON_PATH "ms0:/seplugins/viz_tape/visualizer.rco"
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
int tape_icon_resource_ready(void);
int tape_icon_redirect(void *text, size_t size, uint32_t base);
#endif
