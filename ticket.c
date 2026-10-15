#include <pspiofilemgr.h>
#include <string.h>
#include "safety.h"

/* Explicit persistent opt-in copied during manual installation. Read only;
 * absence or a malformed marker falls back to the optional one-boot ticket. */
int tape_is_persistent(void)
{
    char data[sizeof(TAPE_ENABLE_TEXT)];
    SceUID fd = sceIoOpen(TAPE_ENABLE_PATH, PSP_O_RDONLY, 0);
    int n, closed;
    if (fd < 0) return 0;
    n = sceIoRead(fd, data, sizeof data);
    closed = sceIoClose(fd);
    return n == sizeof(TAPE_ENABLE_TEXT)-1 && closed >= 0 &&
           memcmp(data, TAPE_ENABLE_TEXT, sizeof(TAPE_ENABLE_TEXT)-1) == 0;
}

/* Consume and flush the one-boot ticket BEFORE registering any hook. A failed
 * read/remove/sync disables the test. No ticket on the next boot means no hook. */
int tape_consume_test_ticket(void)
{
    char data[sizeof(TAPE_ARM_TEXT)];
    SceIoStat st;
    SceUID fd = sceIoOpen(TAPE_ARM_PATH, PSP_O_RDONLY, 0);
    int n, closed;
    if (fd < 0) return 0;
    n = sceIoRead(fd, data, sizeof data);
    closed = sceIoClose(fd);
    if (n != sizeof(TAPE_ARM_TEXT)-1 || closed < 0 ||
        memcmp(data, TAPE_ARM_TEXT, sizeof(TAPE_ARM_TEXT)-1)) return 0;
    if (sceIoRemove(TAPE_ARM_PATH) < 0 || sceIoSync("ms0:", 0) < 0) return 0;
    return (unsigned)sceIoGetstat(TAPE_ARM_PATH, &st) == 0x80010002u;
}

