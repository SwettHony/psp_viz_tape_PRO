#include "paths.h"

TapePaths tape_paths = {
    "ms0:", "ms0:/seplugins/viz_tape/status.txt",
    "ms0:/seplugins/viz_tape/boot-guard.txt",
    "ms0:/seplugins/viz_tape/visualizer.rco"
};

int tape_paths_init(const void *args, size_t size)
{
    const unsigned char *path = args;
    unsigned i, internal = 0;
    if (path || size) {
        if (!path || size < 6 || size > 1024) return 0;
        for (i = 0; i < size && path[i]; ++i) {}
        if (i == size || i < 5 || path[2] != '0' || path[3] != ':' || path[4] != '/') return 0;
        /* Character comparisons also avoid ARK's legacy ms0-string rewriting. */
        if ((path[0] == 'e' || path[0] == 'E') && (path[1] == 'f' || path[1] == 'F')) internal = 1;
        else if (!((path[0] == 'm' || path[0] == 'M') && (path[1] == 's' || path[1] == 'S'))) return 0;
    }
    tape_paths.device[0] = internal ? 'e' : 'm';
    tape_paths.device[1] = internal ? 'f' : 's';
    for (i = 0; i < 2; ++i) {
        tape_paths.log[i] = tape_paths.device[i];
        tape_paths.guard[i] = tape_paths.device[i];
        tape_paths.icon[i] = tape_paths.device[i];
    }
    return 1;
}
