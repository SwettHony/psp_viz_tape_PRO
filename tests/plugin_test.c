/* Exercise the production startup/hook/reporter flow without firmware dumps.
 * The icon and guard primitives have separate I/O tests; here their results
 * and scheduling failures are injected at the PSP API boundary. */
#include <assert.h>
#include <stdio.h>
#include "../plugin.c"

static int usb, marker, resource_valid, build_result, guard_result;
static int create_result, start_result, started, deleted, registered;
static int interrupts, redirects, checks, builds, delays, exits, chained;
static int usb_after_check, usb_on_delay, usb_on_start_failure;
static int (*status_entry)(SceSize, void *);
static STMOD_HANDLER installed;
static char messages[4096];
static size_t message_size;
static SceModule music = {
    .nsegment = 2, .text_addr = 0x08800000, .text_size = TAPE_MUSIC_TEXT_SIZE,
    .data_size = TAPE_MUSIC_DATA_SIZE, .bss_size = TAPE_MUSIC_BSS_SIZE,
    .segmentaddr = {0x08800000}, .segmentsize = {TAPE_MUSIC_TEXT_SIZE},
    .modname = "music_player_module"
};

static int old_handler(SceModule *m) { (void)m; ++chained; return 17; }
int sceUsbGetState(void) { return usb; }
int sceKernelCpuSuspendIntr(void) { assert(!interrupts); interrupts = 1; return 0; }
void sceKernelCpuResumeIntr(int irq) { assert(interrupts && irq == 0); interrupts = 0; }
void sceKernelDcacheWritebackAll(void) { assert(interrupts); }
int sceKernelDelayThread(unsigned us)
{
    assert(us == 5000000 && !interrupts);
    if (++delays == usb_on_delay) usb = PSP_USB_ACTIVATED;
    return 0;
}
int sceKernelExitDeleteThread(int result) { assert(result == 0); ++exits; return 0; }
int sceKernelInitKeyConfig(void) { return PSP_INIT_KEYCONFIG_VSH; }
int sceKernelGetModel(void) { return 0; }
unsigned sceKernelDevkitVersion(void) { return 0x06060110; }
SceUID sceKernelCreateThread(const char *name, int (*entry)(SceSize, void *),
                             int priority, int stack, int attr, void *options)
{
    assert(!strcmp(name, "VizTapeStatus") && priority == 0x30 && stack == 8192);
    assert(!attr && !options && !registered);
    status_entry = entry;
    return create_result;
}
int sceKernelStartThread(SceUID tid, SceSize args, void *argp)
{
    assert(tid == create_result && !args && !argp && !registered);
    if (start_result < 0) {
        if (usb_on_start_failure) usb = PSP_USB_ACTIVATED;
        return start_result;
    }
    started = 1;
    return 0;
}
int sceKernelDeleteThread(SceUID tid) { assert(tid == create_result); ++deleted; return 0; }
SceModule *sceKernelFindModuleByName(const char *name) { (void)name; return 0; }
uintptr_t sctrlHENFindFunction(const char *module, const char *library, unsigned nid)
{ (void)module; (void)library; (void)nid; return 0; }
STMOD_HANDLER sctrlHENSetStartModuleHandler(STMOD_HANDLER handler)
{
    STMOD_HANDLER old = installed;
    assert(started && marker); /* A live guard reporter precedes any hook. */
    installed = handler;
    ++registered;
    return old;
}
SceUID sceIoOpen(const char *path, int flags, int mode)
{
    assert(!interrupts && !usb && !strcmp(path, log_path) && mode == 0777);
    assert(flags == (PSP_O_WRONLY|PSP_O_CREAT|PSP_O_TRUNC) ||
           flags == (PSP_O_WRONLY|PSP_O_CREAT|PSP_O_APPEND));
    return 3;
}
int sceIoClose(SceUID fd) { assert(fd == 3); return 0; }
int sceIoWrite(SceUID fd, const void *data, unsigned size)
{
    assert(fd == 3 && message_size + size < sizeof messages && !interrupts && !usb);
    memcpy(messages + message_size, data, size);
    message_size += size;
    messages[message_size] = 0;
    return (int)size;
}
int tape_guard_begin(void) { if (guard_result) marker = 1; return guard_result; }
int tape_guard_finish(void) { assert(!usb && !interrupts); marker = 0; return 1; }
int tape_icon_resource_ready(void)
{
    assert(!interrupts && !usb);
    ++checks;
    if (usb_after_check) usb = PSP_USB_ACTIVATED;
    return resource_valid;
}
int tape_icon_build(void)
{
    assert(!registered && marker && !usb && !interrupts);
    ++builds;
    resource_valid = build_result;
    return build_result;
}
int tape_icon_redirect(void *text, size_t size, uint32_t base)
{
    assert(interrupts && !usb && resource_valid);
    assert((uintptr_t)text == music.text_addr && size == music.text_size && base == music.text_addr);
    ++redirects;
    return 1;
}
int tape_profile_matches(const void *text, size_t size, uint32_t base)
{ (void)text; (void)size; (void)base; return 0; }
uint32_t tape_play_state_address(const void *text, size_t size, uint32_t base)
{ (void)text; (void)size; (void)base; return 0; }

static void reset(void)
{
    usb = marker = resource_valid = started = deleted = registered = 0;
    interrupts = redirects = checks = builds = delays = exits = chained = 0;
    usb_after_check = usb_on_delay = usb_on_start_failure = start_result = 0;
    build_result = guard_result = 1;
    create_result = 8;
    status_entry = 0;
    installed = old_handler;
    previous_handler = 0;
    event = patched_base = seek_event = icon_event = guarded = 0;
    message_size = 0; messages[0] = 0;
}

int main(void)
{
    unsigned i;
    /* The very first Music load after startup sees the newly built icon,
     * without running the reporter or waiting five seconds. */
    reset();
    assert(module_start(0, 0) == 0 && builds == 1 && checks == 1 && !delays);
    assert(strstr(messages, "ICON created"));
    assert(installed(&music) == 17 && redirects == 1 && checks == 2 && chained == 1);
    assert(marker && guarded);

    /* A deleted/corrupted file after USB must not inherit boot-time approval.
     * Neither exiting on USB nor the normal five-minute timeout may disable
     * validation. A valid replacement can be picked up on a later load. */
    for (i = 0; i < 2; ++i) {
        reset(); resource_valid = 1;
        assert(module_start(0, 0) == 0 && !builds);
        usb_on_delay = i ? 0 : 1;
        assert(status_entry(0, 0) == 0 && exits == 1);
        assert(delays == (i ? 60 : 1));
        assert(marker == (i ? 0 : 1));
        usb = 0; resource_valid = 0;
        assert(installed(&music) == 17 && redirects == 0 && checks == 2 && !builds);
        resource_valid = 1;
        assert(installed(&music) == 17 && redirects == 1 && checks == 3);
    }

    /* Active/unknown USB state skips even the read. Activation during a
     * checksum read also prevents the RAM redirect. */
    reset(); resource_valid = 1;
    assert(module_start(0, 0) == 0);
    usb = PSP_USB_ACTIVATED;
    assert(installed(&music) == 17 && checks == 1 && !redirects);
    usb = -1;
    assert(installed(&music) == 17 && checks == 1 && !redirects);
    usb = 0; usb_after_check = 1;
    assert(installed(&music) == 17 && checks == 2 && !redirects);
    usb = usb_after_check = 0;
    assert(installed(&music) == 17 && checks == 3 && redirects == 1);

    /* Both thread failure modes leave the previous plugin's handler intact. */
    for (i = 0; i < 2; ++i) {
        reset(); resource_valid = 1;
        if (i) start_result = -1; else create_result = -1;
        assert(module_start(0, 0) == 1 && installed == old_handler && !registered);
        assert(!started && !marker && !guarded && deleted == (int)i);
        assert(strstr(messages, "status thread could not start; hooks are off"));
    }
    reset(); start_result = -1; usb_on_start_failure = 1;
    assert(module_start(0, 0) == 1 && !registered && marker && guarded);

    /* Build failure preserves stock resource loading; a later valid file
     * still works. Unsupported module layouts never trigger file reads. */
    reset(); build_result = 0;
    assert(module_start(0, 0) == 0 && builds == 1);
    assert(installed(&music) == 17 && !redirects);
    resource_valid = 1;
    assert(installed(&music) == 17 && redirects == 1);
    --music.text_size;
    assert(installed(&music) == 17 && checks == 3 && redirects == 1);
    ++music.text_size;
    assert(installed(0) == 17 && checks == 3);
    reset(); guard_result = 0;
    assert(module_start(0, 0) == 1 && !registered && !checks && !builds);

    puts("Plugin lifecycle: immediate first icon, per-load validation after USB/timeout, USB exclusion, thread failures and handler chaining passed.");
    return 0;
}
