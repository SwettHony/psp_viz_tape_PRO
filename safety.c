#include "safety.h"
int tape_user_range(uintptr_t address, size_t size)
{
    return !(address & 3) && address >= 0x08800000u && address < 0x0a000000u &&
           size > 0 && size <= 0x0a000000u-address;
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
