#include <pspiofilemgr.h>
#include <string.h>
#include "icon.h"
#include "icon_resource.h"
#include "safety.h"

/* Read-only preflight at startup and before each Music-module redirect.
 * Runs with interrupts enabled, never inside a draw callback.
 * Manual copies can be verified with SHA256SUMS; this check catches missing,
 * partial, stale or accidentally damaged copies before changing a RAM path. */
int tape_icon_resource_ready(void)
{
    unsigned char chunk[512];
    uint32_t hash = 2166136261u, total = 0;
    int n, i, valid = 1;
    SceUID fd = sceIoOpen(TAPE_ICON_PATH, PSP_O_RDONLY, 0);
    if (fd < 0) return 0;
    while ((n = sceIoRead(fd, chunk, sizeof chunk)) > 0) {
        if ((unsigned)n > sizeof chunk || (unsigned)n > TAPE_ICON_SIZE-total) {
            valid = 0; break;
        }
        total += (unsigned)n;
        for (i = 0; i < n; ++i) hash = (hash^chunk[i])*16777619u;
    }
    if (n < 0) valid = 0;
    if (sceIoClose(fd) < 0) valid = 0;
    return valid && total == TAPE_ICON_SIZE && hash == TAPE_ICON_FNV;
}

/* A single data-string replacement in the captured 6.61 Music player. The
 * stock path is a RAM signature here; icon_build.c opens it read-only to
 * create the card copy. No instructions, imports, PAF widgets or firmware
 * files are modified. */
const char tape_stock_icon_path[] = "flash0:/vsh/resource/visualizer_plugin.rco";

int tape_icon_redirect(void *text, size_t size, uint32_t base)
{
    const char *original = tape_stock_icon_path;
    const size_t original_size = sizeof tape_stock_icon_path;
    const char *replacement = TAPE_ICON_PATH;
    const size_t replacement_size = sizeof tape_paths.icon;
    static const uint32_t prefix[] = {
        0x27bdff70, 0xafb3007c, 0x00809821, 0x03a02021,
        0xafbf0080, 0xafb20078, 0x27b2000c, 0xafb10074
    };
    const uint32_t *words = text;
    uint32_t path;
    unsigned i;
    unsigned char *bytes = text;
    if (!text || (uintptr_t)text%4 || size != TAPE_MUSIC_TEXT_SIZE ||
        !tape_user_range(base, TAPE_MUSIC_TEXT_SIZE) || replacement_size > original_size) return 0;
    path = base+TAPE_MUSIC_ICON_PATH;
    for (i = 0; i < sizeof prefix/sizeof prefix[0]; ++i)
        if (words[TAPE_MUSIC_LOADER/4+i] != prefix[i]) return 0;
    /* Relocated lui/addiu pair must pass this exact string to the native
     * string constructor while assembling the visualizer resource load. */
    if (words[TAPE_MUSIC_PATH_LUI/4] != (0x3c050000u | ((path+0x8000u)>>16)) ||
        words[TAPE_MUSIC_PATH_LUI/4+1] != 0x02202021u ||
        words[TAPE_MUSIC_STRING_CALL/4] != (0x0c000000u | ((base+TAPE_MUSIC_STRING_CTOR)>>2)) ||
        words[TAPE_MUSIC_STRING_CALL/4+1] != (0x24a50000u | (path&0xffffu)) ||
        words[TAPE_MUSIC_EPILOGUE/4] != 0x03e00008u || words[TAPE_MUSIC_EPILOGUE/4+1] != 0x27bd0090u ||
        memcmp(bytes+TAPE_MUSIC_ICON_PATH, original, original_size)) return 0;
    for (i = 0; i < original_size; ++i)
        bytes[TAPE_MUSIC_ICON_PATH+i] = i < replacement_size ? replacement[i] : 0;
    return 1;
}
