#include "../paths.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void check(const char *device)
{
    char expected[80];
    assert(!strcmp(tape_paths.device, device));
    snprintf(expected, sizeof expected, "%s/seplugins/viz_tape/status.txt", device);
    assert(!strcmp(TAPE_LOG_PATH, expected));
    snprintf(expected, sizeof expected, "%s/seplugins/viz_tape/boot-guard.txt", device);
    assert(!strcmp(TAPE_GUARD_PATH, expected));
    snprintf(expected, sizeof expected, "%s/seplugins/viz_tape/visualizer.rco", device);
    assert(!strcmp(TAPE_ICON_PATH, expected));
    assert(strlen(TAPE_ICON_PATH) < strlen("flash0:/vsh/resource/visualizer_plugin.rco"));
}

int main(void)
{
    const char *valid[] = { "ms0:/seplugins/viz_tape/viz_tape.prx", "ef0:/seplugins/viz_tape/viz_tape.prx",
                            "MS0:/SEPLUGINS/viz_tape/viz_tape.prx", "EF0:/seplugins/viz_tape/viz_tape.prx" };
    const char *invalid[] = { "", "ms0:", "ms0:relative", "flash0:/x", "ef1:/x", "seplugins/viz_tape.prx" };
    unsigned i;
    for (i = 0; i < sizeof valid/sizeof valid[0]; ++i) {
        assert(tape_paths_init(valid[i], strlen(valid[i])+1));
        check(i % 2 ? "ef0:" : "ms0:");
    }
    for (i = 0; i < sizeof invalid/sizeof invalid[0]; ++i) {
        TapePaths before = tape_paths;
        assert(!tape_paths_init(invalid[i], strlen(invalid[i])+1));
        assert(!memcmp(&before, &tape_paths, sizeof before));
    }
    assert(!tape_paths_init("ef0:/x", 6)); /* Missing terminator inside args. */
    assert(!tape_paths_init(0, 100));
    assert(!tape_paths_init("ef0:/x", 0));
    assert(!tape_paths_init("ef0:/x", 1025));
    assert(tape_paths_init(0, 0)); check("ms0:");
    puts("Paths: Memory Stick and Go internal storage, uppercase devices and bounded invalid-argument rejection passed.");
    return 0;
}
