#include <pspiofilemgr.h>
#include "../safety.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int exists, failure, step, persistent;
SceUID sceIoOpen(const char *path, int flags, int mode)
{
    assert(!strcmp(path, persistent ? TAPE_ENABLE_PATH : TAPE_ARM_PATH) && flags == PSP_O_RDONLY && mode == 0);
    ++step;
    return exists && failure != 1 ? 10 : -1;
}
int sceIoRead(SceUID fd, void *out, unsigned size)
{
    assert(fd == 10 && step++ == 1);
    if (failure == 2) return -1;
    const char *text = persistent ? TAPE_ENABLE_TEXT : TAPE_ARM_TEXT;
    memcpy(out, text, strlen(text)+1);
    if (failure == 3) ((char *)out)[0] ^= 1;
    return failure == 4 ? (int)size : (int)strlen(text);
}
int sceIoClose(SceUID fd) { assert(fd == 10 && step++ == 2); return failure == 5 ? -1 : 0; }
int sceIoRemove(const char *path)
{
    assert(!strcmp(path, TAPE_ARM_PATH) && step++ == 3);
    if (failure == 6) return -1;
    exists = 0; return 0;
}
int sceIoSync(const char *path, unsigned flags)
{
    assert(!strcmp(path, "ms0:") && flags == 0 && step++ == 4);
    return failure == 7 ? -1 : 0;
}
int sceIoGetstat(const char *path, SceIoStat *st)
{
    (void)st;
    assert(!strcmp(path, TAPE_ARM_PATH) && step++ == 5);
    if (failure == 8) return 0; /* File unexpectedly still exists. */
    if (failure == 9) return -1; /* Unknown device error is not absence. */
    return (int)0x80010002u;
}
int main(void)
{
    for (failure = 1; failure <= 9; ++failure) {
        exists = 1; step = 0;
        assert(tape_consume_test_ticket() == 0);
    }
    failure = 0; exists = 1; step = 0;
    assert(tape_consume_test_ticket() == 1 && !exists && step == 6);
    step = 0;
    assert(tape_consume_test_ticket() == 0); /* Subsequent boot is not armed. */
    persistent = 1;
    for (failure = 1; failure <= 5; ++failure) {
        exists = 1; step = 0;
        assert(!tape_is_persistent());
        assert(exists); /* Persistent checks are strictly read-only. */
    }
    failure = 0; exists = 1; step = 0;
    assert(tape_is_persistent() && exists && step == 3);
    step = 0;
    assert(tape_is_persistent() && exists && step == 3); /* Next boot still enabled. */
    exists = 0; step = 0;
    assert(!tape_is_persistent());
    puts("Activation: persistent marker survives repeated starts; absent/bad markers rejected; optional one-use ticket and all I/O failures checked.");
    return 0;
}
