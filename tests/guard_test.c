#include <pspiofilemgr.h>
#include "../safety.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Simulated card: the guard file plus injectable failures. */
static int exists, failure, open_files;
static char contents[64];
enum { STAT_ERROR = 1, OPEN_FAIL, SHORT_WRITE, CLOSE_FAIL, SYNC_FAIL, REMOVE_FAIL };

int sceIoGetstat(const char *path, SceIoStat *st)
{
    (void)st;
    assert(!strcmp(path, TAPE_GUARD_PATH));
    if (failure == STAT_ERROR) return -1; /* Unknown device error is not absence. */
    return exists ? 0 : (int)0x80010002u;
}
SceUID sceIoOpen(const char *path, int flags, int mode)
{
    assert(!strcmp(path, TAPE_GUARD_PATH) && mode == 0777);
    assert(flags == (PSP_O_WRONLY|PSP_O_CREAT|PSP_O_TRUNC));
    if (failure == OPEN_FAIL) return -1;
    exists = 1; contents[0] = 0; ++open_files;
    return 10;
}
int sceIoWrite(SceUID fd, const void *data, unsigned size)
{
    assert(fd == 10 && size < sizeof contents);
    if (failure == SHORT_WRITE) size /= 2;
    memcpy(contents, data, size); contents[size] = 0;
    return (int)size;
}
int sceIoClose(SceUID fd) { assert(fd == 10); --open_files; return failure == CLOSE_FAIL ? -1 : 0; }
int sceIoRemove(const char *path)
{
    assert(!strcmp(path, TAPE_GUARD_PATH));
    if (failure == REMOVE_FAIL || !exists) return -1;
    exists = 0; return 0;
}
int sceIoSync(const char *path, unsigned flags)
{
    assert(!strcmp(path, "ms0:") && flags == 0);
    return failure == SYNC_FAIL ? -1 : 0;
}
int sceIoRead(SceUID fd, void *out, unsigned size) { (void)fd; (void)out; (void)size; assert(0); return -1; }

int main(void)
{
    int start;
    /* Clean start: the guard is written, then cleared after a healthy run. */
    exists = 0; failure = 0;
    assert(tape_guard_begin() == 1 && exists && !strcmp(contents, TAPE_GUARD_TEXT));
    assert(tape_guard_finish() == 1 && !exists);
    /* A fault leaves the guard: the next start is skipped and the marker is
     * removed, so the start after that hooks again. No endless boot loop. */
    assert(tape_guard_begin() == 1 && exists);
    assert(tape_guard_begin() == 0 && !exists);
    assert(tape_guard_begin() == 1 && exists);
    /* Repeated faults alternate hooked and skipped starts. */
    for (start = 0; start < 6; ++start)
        assert(tape_guard_begin() == (start % 2 ? 1 : 0));
    /* Any failure to write the guard skips the hooks (fail closed). */
    for (failure = STAT_ERROR; failure <= SYNC_FAIL; ++failure) {
        exists = 0;
        assert(tape_guard_begin() == 0);
        assert(open_files == 0);
    }
    failure = REMOVE_FAIL; exists = 1;
    assert(tape_guard_finish() == 0);
    failure = SYNC_FAIL; exists = 1;
    assert(tape_guard_finish() == 0);
    puts("Boot guard: clean starts clear it; a faulted start skips only the next start; write failures skip hooks.");
    return 0;
}
