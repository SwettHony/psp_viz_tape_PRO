#include "profile.h"
#include "safety.h"

/* Relocation-independent checks from a stock 6.61 visualizer capture.
 * These are a small code signature and the final brown-line effect's complete vtable layout. */
int tape_profile_matches(const void *text, size_t size, uint32_t base)
{
    static const uint32_t slots[] = {
        0x8594, 0x86e4, 0x103c4, 0x91ec, 0x103cc, 0x103d4, 0x9580,
        0x883c, 0x8900, 0x896c, 0x10400, 0x10408, 0x10418, 0x1042c, 0x89a8
    };
    static const uint32_t prefix[] = {
        0x27bdff90, 0xafb3004c, 0x34028000, 0x00a09821,
        0xafb20048, 0x00809021, 0x00822021, 0xafbf0054
    };
    static const uint32_t dispatch[] = {
        0x8e040040, 0x8c830000, 0x8c62000c, 0x0040f809,
        0x26050030, 0x8e050040, 0xa200003c, 0x8ca30000
    };
    const uint32_t *words = text;
    unsigned i;
    if (!text || (uintptr_t)text % 4 || size != TAPE_TEXT_SIZE ||
        !tape_user_range(base, TAPE_TEXT_SIZE)) return 0;
    for (i = 0; i < sizeof slots / sizeof slots[0]; ++i)
        if (words[TAPE_VTABLE/4+i] != base + slots[i]) return 0;
    for (i = 0; i < sizeof prefix / sizeof prefix[0]; ++i)
        if (words[TAPE_ORIGINAL_DRAW/4+i] != prefix[i]) return 0;
    for (i = 0; i < sizeof dispatch / sizeof dispatch[0]; ++i)
        if (words[TAPE_DISPATCH/4+i] != dispatch[i]) return 0;
    /* The final effect uses the same base-object fade at +0x24. */
    return words[TAPE_FADE_LOAD/4] == 0xc6440024 &&
           words[0x103c4/4] == 0x03e00008 && words[0x103c8/4] == 0;
}

/* The getter is "lui v1,%hi(state); jr ra; lw v0,%lo(state)(v1)", and the
 * message handler stores fast-forward (2) and rewind (3) through the same pair. */
uint32_t tape_play_state_address(const void *text, size_t size, uint32_t base)
{
    const uint32_t *words = text;
    uint32_t state = base + TAPE_PLAY_STATE;
    uint32_t hi = (state + 0x8000u) >> 16, lo = state & 0xffffu;
    if (!text || (uintptr_t)text % 4 || size != TAPE_TEXT_SIZE ||
        !tape_user_range(base, TAPE_TEXT_SIZE)) return 0;
    if (words[TAPE_STATE_GETTER/4] != (0x3c030000u | hi) ||
        words[TAPE_STATE_GETTER/4+1] != 0x03e00008u ||
        words[TAPE_STATE_GETTER/4+2] != (0x8c620000u | lo)) return 0;
    /* Message 3: li v1,2; lui v0,%hi; j; sw v1,%lo(v0). Message 4 stores 3. */
    if (words[0x2064/4] != 0x24030002u || words[0x2068/4] != (0x3c020000u | hi) ||
        words[0x2070/4] != (0xac430000u | lo) || words[0x2078/4] != 0x24030003u) return 0;
    return state;
}
