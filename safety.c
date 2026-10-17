#include "safety.h"
static uintptr_t user_base = 0x08800000u;
static size_t user_size = 0x01800000u;

int tape_set_user_memory(uintptr_t base, size_t size)
{
    user_size = 0;
    /* Cached user RAM on retail PSPs, bounded by the largest 64 MB model.
     * Never widen this range merely because a model has more physical RAM. */
    if ((base & 3) || (size & 3) || base < 0x08800000u || base >= 0x0c000000u ||
        !size || size > 0x0c000000u - base) return 0;
    user_base = base;
    user_size = size;
    return 1;
}

int tape_user_range(uintptr_t address, size_t size)
{
    return !(address & 3) && address >= user_base && address-user_base < user_size &&
           size > 0 && size <= user_size-(address-user_base);
}
int tape_range_contains(uintptr_t base, size_t span, uintptr_t address, size_t size)
{
    return tape_user_range(base, span) && tape_user_range(address, size) &&
           address >= base && address-base < span && size <= span-(address-base);
}
int tape_stub_matches(const uint32_t *stub, uintptr_t address, uintptr_t target)
{
    if (!stub || !tape_user_range(address, 8) || !tape_user_range(target, 4)) return 0;
    return (stub[0] & 0xfc000000u) == 0x08000000u && stub[1] == 0 &&
           (((address+4) & 0xf0000000u) | ((stub[0] & 0x03ffffffu) << 2)) == target;
}
