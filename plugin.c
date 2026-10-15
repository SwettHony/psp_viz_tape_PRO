#include <pspkernel.h>
#include <psploadcore.h>
#include <pspsysmem_kernel.h>
#include <pspintrman.h>
#include <pspinit.h>
#include <pspusb.h>
#include <systemctrl.h>
#include <stdio.h>
#include <string.h>
#include "profile.h"
#include "runtime.h"
#include "safety.h"
#include "icon.h"

PSP_MODULE_INFO("viz_tape", PSP_MODULE_KERNEL, 1, 0);
#define VIZ_TAPE_VERSION "1.0"
PSP_NO_CREATE_MAIN_THREAD();

static STMOD_HANDLER previous_handler;
static volatile TapeBridge * volatile renderer_bridge;
static volatile SceUID renderer_uid = -1;
static volatile unsigned event, patched_base, seek_event;
static int icon_ready;
static volatile unsigned icon_event;
static const char *const icon_events[] = {
    "ICON skipped: resource missing or failed validation",
    "ICON ready: waiting for Music player",
    "ICON cassette resource selected in RAM",
    "ICON skipped: Music player layout/path mismatch"
};
static const char *const seek_events[] = {
    "", "SEEK fast-forward/rewind state found", "SEEK skipped: playback state layout mismatch"
};
static const char log_path[] = "ms0:/seplugins/viz_tape/status.txt";
static const char *const events[] = {
    "waiting for visualizer", "PATCHED final brown-line slot 7",
    "SKIPPED: firmware layout mismatch", "SKIPPED: renderer export/ABI mismatch",
    "SKIPPED: PAF exports do not match native imports", "SKIPPED: draw slot changed"
};

static volatile int guarded;

/* An error result is treated as "maybe active": never write to the card then. */
static int usb_active(void)
{
    int usb = sceUsbGetState();
    return usb < 0 || (usb & PSP_USB_ACTIVATED);
}

static void log_message(const char *message)
{
    if (usb_active()) return;
    SceUID fd = sceIoOpen(log_path, PSP_O_WRONLY|PSP_O_CREAT|PSP_O_APPEND, 0777);
    if (fd >= 0) { sceIoWrite(fd, message, strlen(message)); sceIoClose(fd); }
}
#define report(...) do { char msg[256]; snprintf(msg, sizeof msg, __VA_ARGS__); log_message(msg); } while (0)

static int module_contains(SceModule *m, uintptr_t p, size_t size)
{
    unsigned i;
    if (!m || m->nsegment < 1 || m->nsegment > 4) return 0;
    for (i = 0; i < m->nsegment; ++i)
        if (tape_range_contains(m->segmentaddr[i], m->segmentsize[i], p, size)) return 1;
    return 0;
}

static uintptr_t resolve_paf(SceModule *viz, SceModule *paf, unsigned nid, unsigned stub_offset)
{
    uintptr_t p = sctrlHENFindFunction("scePaf_Module", "scePaf", nid);
    uintptr_t stub = viz->text_addr + stub_offset;
    if (!tape_range_contains(paf->text_addr, paf->text_size, p, 4) ||
        !tape_stub_matches((const uint32_t *)stub, stub, p)) return 0;
    return p;
}

static void patch_visualizer(SceModule *module)
{
    SceModule *renderer, *paf;
    uint32_t *slot;
    uintptr_t draw, bridge_address;
    volatile TapeBridge *bridge;
    TapeBindings bindings;
    int irq;
    if (module->nsegment != 2 || module->text_size != TAPE_TEXT_SIZE ||
        module->data_size != TAPE_DATA_SIZE || module->bss_size != TAPE_BSS_SIZE ||
        module->segmentaddr[0] != module->text_addr ||
        module->segmentsize[0] != TAPE_TEXT_SIZE ||
        !tape_user_range(module->text_addr, TAPE_TEXT_SIZE) ||
        !tape_profile_matches((void *)module->text_addr, module->text_size, module->text_addr)) {
        event = 2; return;
    }
    renderer = sceKernelFindModuleByName("viz_tape_render");
    paf = sceKernelFindModuleByName("scePaf_Module");
    draw = sctrlHENFindFunction("viz_tape_render", "VizTape", 0xeab11153);
    /* ARK's export lookup searches both function and variable exports. */
    bridge_address = sctrlHENFindFunction("viz_tape_render", "VizTape", 0x99f748f8);
    if (!renderer || !paf ||
        !tape_range_contains(renderer->text_addr, renderer->text_size, draw, 4) ||
        !module_contains(renderer, bridge_address, sizeof(TapeBridge))) {
        event = 3; return;
    }
    bridge = (volatile TapeBridge *)bridge_address;
    if (bridge->abi != TAPE_ABI || bridge->size != sizeof(TapeBridge) ||
        bridge->status.abi != TAPE_ABI) { event = 3; return; }
    bindings.abi = TAPE_ABI;
    bindings.draw = (TapeDrawArray)resolve_paf(module, paf, TAPE_NID_DRAW, TAPE_STUB_DRAW);
    bindings.enable = (TapeStateFunction)resolve_paf(module, paf, TAPE_NID_ENABLE, TAPE_STUB_ENABLE);
    bindings.disable = (TapeStateFunction)resolve_paf(module, paf, TAPE_NID_DISABLE, TAPE_STUB_DISABLE);
    bindings.blend = (TapeBlendFunction)resolve_paf(module, paf, TAPE_NID_BLEND, TAPE_STUB_BLEND);
    if (!bindings.draw || !bindings.enable || !bindings.disable || !bindings.blend) {
        event = 4; return;
    }
    /* Optional: without a verified state word, seeking simply winds normally. */
    {
        uint32_t state = tape_play_state_address((void *)module->text_addr,
                                                 module->text_size, module->text_addr);
        bindings.play_state = state && module_contains(module, state, 4) &&
            !tape_range_contains(module->text_addr, module->text_size, state, 4) ?
            (const volatile uint32_t *)state : 0;
        seek_event = bindings.play_state ? 1 : 2;
    }
    slot = (uint32_t *)(module->text_addr + TAPE_DRAW_SLOT);
    irq = sceKernelCpuSuspendIntr();
    if (*slot != module->text_addr + TAPE_ORIGINAL_DRAW) {
        sceKernelCpuResumeIntr(irq); event = 5; return;
    }
    bridge->ready = 0;
    bridge->api = bindings;
    bridge->ready = TAPE_READY;
    *slot = draw;
    renderer_bridge = bridge;
    renderer_uid = renderer->modid;
    sceKernelDcacheWritebackAll();
    sceKernelCpuResumeIntr(irq);
    patched_base = module->text_addr;
    event = 1;
}

static int on_module_start(SceModule *module)
{
    int result = previous_handler ? previous_handler(module) : 0;
    if (module && icon_ready && strcmp(module->modname, "music_player_module") == 0) {
        int irq;
        icon_event = 3;
        if (module->nsegment == 2 && module->text_size == TAPE_MUSIC_TEXT_SIZE &&
            module->data_size == TAPE_MUSIC_DATA_SIZE && module->bss_size == TAPE_MUSIC_BSS_SIZE &&
            module->segmentaddr[0] == module->text_addr &&
            module->segmentsize[0] == TAPE_MUSIC_TEXT_SIZE &&
            tape_user_range(module->text_addr, TAPE_MUSIC_TEXT_SIZE)) {
            irq = sceKernelCpuSuspendIntr();
            if (tape_icon_redirect((void *)module->text_addr, module->text_size, module->text_addr)) {
                sceKernelDcacheWritebackAll();
                icon_event = 2;
            }
            sceKernelCpuResumeIntr(irq);
        }
    }
    if (module && strcmp(module->modname, "visualizer_module") == 0) patch_visualizer(module);
    return result;
}

static int reporter(SceSize args, void *argp)
{
    unsigned i, last_event = ~0u, last_icon_event = ~0u, last_seek_event = ~0u;
    (void)args; (void)argp;
    for (i = 0; i < 60; ++i) {
        TapeStatus snapshot;
        SceModule *m;
        volatile TapeBridge *bridge;
        unsigned current;
        int valid = 0, irq;
        sceKernelDelayThread(5000000);
        /* Stop card logging when USB storage is activated for the host. */
        if (usb_active()) break;
        if (guarded && (i+1)*5 >= TAPE_GUARD_SECONDS) {
            guarded = 0;
            if (!tape_guard_finish()) report("WARNING: could not clear boot-guard.txt\n");
        }
        current = icon_event;
        if (current != last_icon_event && current < sizeof icon_events/sizeof icon_events[0]) {
            report("%s\n", icon_events[current]);
            last_icon_event = current;
        }
        current = seek_event;
        if (current != last_seek_event && current && current < sizeof seek_events/sizeof seek_events[0]) {
            report("%s\n", seek_events[current]);
            last_seek_event = current;
        }
        current = event;
        if (current != last_event && current < sizeof events / sizeof events[0]) {
            report("%s; visualizer=%08X\n", events[current], patched_base);
            last_event = current;
        }
        irq = sceKernelCpuSuspendIntr();
        m = sceKernelFindModuleByUID(renderer_uid);
        bridge = renderer_bridge;
        if (bridge && module_contains(m, (uintptr_t)bridge, sizeof *bridge)) {
            snapshot = bridge->status;
            valid = 1;
        }
        sceKernelCpuResumeIntr(irq);
        if (valid && (i < 3 || i % 3 == 0))
            report("frames=%u audio=%u ff=%u rew=%u rectangles=%u errors=%u alpha=%u render_us=%u max_us=%u\n",
                   (unsigned)snapshot.frames, (unsigned)snapshot.audio_frames,
                   (unsigned)snapshot.forward_frames, (unsigned)snapshot.rewind_frames,
                   (unsigned)snapshot.max_rectangles, (unsigned)snapshot.errors,
                   (unsigned)snapshot.last_alpha, (unsigned)snapshot.last_frame_us,
                   (unsigned)snapshot.max_frame_us);
    }
    return sceKernelExitDeleteThread(0);
}


int module_start(SceSize args, void *argp)
{
    SceUID fd, tid;
    unsigned version;
    int persistent;
    (void)args; (void)argp;
    if (sceKernelInitKeyConfig() != PSP_INIT_KEYCONFIG_VSH || sceKernelGetModel() != 0) return 1;
    version = sceKernelDevkitVersion();
    if (version != 0x06060010 && version != 0x06060110) return 1;
    if (usb_active()) return 1;
    persistent = tape_is_persistent();
    if (!persistent && !tape_consume_test_ticket()) return 1;
    fd = sceIoOpen(log_path, PSP_O_WRONLY|PSP_O_CREAT|PSP_O_TRUNC, 0777);
    if (fd >= 0) sceIoClose(fd);
    if (persistent && !tape_guard_begin()) {
        report("Viz Tape " VIZ_TAPE_VERSION " SKIPPED: the previous start did not finish cleanly "
               "(or boot-guard.txt could not be written). Hooks are off for this "
               "start only; the next start tries again.\n");
        return 1;
    }
    guarded = persistent;
    report("Viz Tape " VIZ_TAPE_VERSION " %s; devkit=%08X model=0\n",
           persistent ? "persistent mode" : "one-boot test; ticket consumed", version);
    icon_ready = tape_icon_resource_ready();
    icon_event = icon_ready ? 1 : 0;
    previous_handler = sctrlHENSetStartModuleHandler(on_module_start);
    tid = sceKernelCreateThread("VizTapeStatus", reporter, 0x30, 4096, 0, 0);
    if (tid >= 0 && sceKernelStartThread(tid, 0, 0) < 0) {
        sceKernelDeleteThread(tid);
        tid = -1;
    }
    /* Without the reporter nothing would clear the guard, and every other
     * start would be skipped. Clear it now; the hooks themselves are unchanged. */
    if (tid < 0 && guarded) { guarded = 0; tape_guard_finish(); }
    return 0;
}

/* ARK's chained handler can remain referenced by later plugins. */
int module_stop(SceSize args, void *argp) { (void)args; (void)argp; return -1; }
