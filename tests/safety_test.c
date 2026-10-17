#include "../safety.h"
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
int main(void)
{
    uint32_t stub[2] = {0x08000000u | (0x08801000u >> 2), 0};
    assert(tape_user_range(0x08800000, 0x01800000));
    assert(tape_user_range(0x09fffffc, 4));
    assert(!tape_user_range(0x09fffffc, 5));
    assert(!tape_user_range(0x08800001, 4));
    assert(!tape_user_range(0x88000000, 4));
    assert(!tape_user_range(0x08800000, SIZE_MAX));
    assert(!tape_user_range(UINTPTR_MAX, 1));
    assert(!tape_user_range(0x08800000, 0));
    assert(tape_range_contains(0x08801000, 256, 0x088010fc, 4));
    assert(!tape_range_contains(0x08801000, 256, 0x088010fc, 8));
    assert(!tape_range_contains(0x08801000, 256, 0x08800ffc, 4));
    assert(!tape_range_contains(0x08801000, SIZE_MAX, 0x08801000, 4));
    assert(tape_stub_matches(stub, 0x09d4d368, 0x08801000));
    assert(!tape_stub_matches(stub, 0x09d4d368, 0x08801004));
    stub[1] = 0x24020000; /* Unexpected delay-slot instruction. */
    assert(!tape_stub_matches(stub, 0x09d4d368, 0x08801000));
    stub[1] = 0; stub[0] |= 0x04000000; /* JAL instead of resolved J stub. */
    assert(!tape_stub_matches(stub, 0x09d4d368, 0x08801000));
    assert(tape_set_user_memory(0x08800000, 0x03800000));
    assert(tape_user_range(0x0a000000, 4));
    assert(tape_user_range(0x0bfffffc, 4));
    assert(!tape_user_range(0x0bfffffc, 8));
    assert(!tape_user_range(0x0c000000, 4));
    assert(tape_range_contains(0x0b000000, 0x100000, 0x0b000040, 16));
    stub[0] = 0x08000000u | (0x0b001000u >> 2);
    assert(tape_stub_matches(stub, 0x0a001000, 0x0b001000));
    assert(tape_set_user_memory(0x08800000, 0x01800000));
    assert(!tape_user_range(0x0a000000, 4));
    assert(!tape_set_user_memory(0x88000000, 0x01800000));
    assert(!tape_user_range(0x08800000, 4)); /* Invalid configuration closes access. */
    assert(!tape_set_user_memory(0x08800000, SIZE_MAX));
    assert(!tape_set_user_memory(0x08800000, 0));
    assert(!tape_set_user_memory(0x08800001, 0x1000));
    assert(!tape_set_user_memory(0x08800000, 0x1001));
    assert(!tape_set_user_memory(0x08700000, 0x1000));
    assert(!tape_set_user_memory(0x08800000, 0x03800004));
    puts("Safety: complete address ranges, overflow, module ownership and import targets checked.");
    return 0;
}
