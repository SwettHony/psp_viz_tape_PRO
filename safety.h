#ifndef VIZ_TAPE_SAFETY_H
#define VIZ_TAPE_SAFETY_H
#include <stddef.h>
#include <stdint.h>
#include "paths.h"
#define TAPE_GUARD_TEXT "VIZ_TAPE_STARTING\n"
/* Seconds of clean running before the boot guard is cleared. */
#define TAPE_GUARD_SECONDS 15
int tape_guard_begin(void);
int tape_guard_finish(void);
/* Set once before kernel hooks, and from the verified bridge before drawing.
 * The kernel supplies the actual user partition rather than a model guess. */
int tape_set_user_memory(uintptr_t base, size_t size);
int tape_user_range(uintptr_t address, size_t size);
int tape_range_contains(uintptr_t base, size_t span, uintptr_t address, size_t size);
int tape_stub_matches(const uint32_t *stub, uintptr_t stub_address, uintptr_t target);
#endif
