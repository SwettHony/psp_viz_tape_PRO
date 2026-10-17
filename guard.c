#include <pspiofilemgr.h>
#include <string.h>
#include "safety.h"

/* Persistent mode keeps a marker on the card while XMB is starting. If it is
 * still present at the next start, that start ended in a fault or power loss:
 * skip the hooks once and remove the marker, so a fault cannot loop forever. */
int tape_guard_begin(void)
{
    SceIoStat st;
    SceUID fd;
    int written, closed;
    int found = sceIoGetstat(TAPE_GUARD_PATH, &st);
    if (found >= 0) {
        sceIoRemove(TAPE_GUARD_PATH);
        sceIoSync(tape_paths.device, 0);
        return 0;
    }
    if ((unsigned)found != 0x80010002u) return 0;
    fd = sceIoOpen(TAPE_GUARD_PATH, PSP_O_WRONLY|PSP_O_CREAT|PSP_O_TRUNC, 0777);
    if (fd < 0) return 0;
    written = sceIoWrite(fd, TAPE_GUARD_TEXT, sizeof(TAPE_GUARD_TEXT)-1);
    closed = sceIoClose(fd);
    return written == sizeof(TAPE_GUARD_TEXT)-1 && closed >= 0 &&
           sceIoSync(tape_paths.device, 0) >= 0;
}

/* Called once startup has run cleanly; the next start hooks normally. */
int tape_guard_finish(void)
{
    return sceIoRemove(TAPE_GUARD_PATH) >= 0 && sceIoSync(tape_paths.device, 0) >= 0;
}
