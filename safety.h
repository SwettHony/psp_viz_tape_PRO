#ifndef VIZ_TAPE_SAFETY_H
#define VIZ_TAPE_SAFETY_H
#include <stddef.h>
#include <stdint.h>
#define TAPE_ARM_PATH "ms0:/seplugins/viz_tape/test-once.txt"
#define TAPE_ARM_TEXT "VIZ_TAPE_TEST_ONCE_03\n"
#define TAPE_ENABLE_PATH "ms0:/seplugins/viz_tape/enabled.txt"
#define TAPE_ENABLE_TEXT "VIZ_TAPE_ENABLED\n"
#define TAPE_GUARD_PATH "ms0:/seplugins/viz_tape/boot-guard.txt"
#define TAPE_GUARD_TEXT "VIZ_TAPE_STARTING\n"
/* Seconds of clean running before the boot guard is cleared. */
#define TAPE_GUARD_SECONDS 15
int tape_is_persistent(void);
int tape_consume_test_ticket(void);
int tape_guard_begin(void);
int tape_guard_finish(void);
int tape_user_range(uintptr_t address, size_t size);
int tape_range_contains(uintptr_t base, size_t span, uintptr_t address, size_t size);
int tape_stub_matches(const uint32_t *stub, uintptr_t stub_address, uintptr_t target);
#endif
