/* Synthetic copies of the reviewed signatures test relocation and bounds.
 * They do not stand in for testing a different model's actual firmware. */
#include <pspiofilemgr.h>
#include "../icon.h"
#include "../profile.h"
#include "../safety.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

SceUID sceIoOpen(const char *p, int f, int m) { (void)p; (void)f; (void)m; assert(0); return -1; }
int sceIoRead(SceUID f, void *p, unsigned n) { (void)f; (void)p; (void)n; assert(0); return -1; }
int sceIoClose(SceUID f) { (void)f; assert(0); return -1; }

static void visualizer_fixture(uint32_t *w, uint32_t base)
{
    static const unsigned slots[] = {0x8594,0x86e4,0x103c4,0x91ec,0x103cc,0x103d4,0x9580,
        0x883c,0x8900,0x896c,0x10400,0x10408,0x10418,0x1042c,0x89a8};
    static const uint32_t prefix[] = {0x27bdff90,0xafb3004c,0x34028000,0x00a09821,
        0xafb20048,0x00809021,0x00822021,0xafbf0054};
    static const uint32_t dispatch[] = {0x8e040040,0x8c830000,0x8c62000c,0x0040f809,
        0x26050030,0x8e050040,0xa200003c,0x8ca30000};
    uint32_t state = base + TAPE_PLAY_STATE, hi = (state+0x8000)>>16, lo = state&0xffff;
    unsigned i;
    memset(w, 0, TAPE_TEXT_SIZE);
    for (i = 0; i < 15; ++i) w[TAPE_VTABLE/4+i] = base+slots[i];
    memcpy(w+TAPE_ORIGINAL_DRAW/4, prefix, sizeof prefix);
    memcpy(w+TAPE_DISPATCH/4, dispatch, sizeof dispatch);
    w[TAPE_FADE_LOAD/4] = 0xc6440024;
    w[0x103c4/4] = 0x03e00008;
    w[TAPE_STATE_GETTER/4] = 0x3c030000|hi;
    w[TAPE_STATE_GETTER/4+1] = 0x03e00008;
    w[TAPE_STATE_GETTER/4+2] = 0x8c620000|lo;
    w[0x2064/4] = 0x24030002;
    w[0x2068/4] = 0x3c020000|hi;
    w[0x2070/4] = 0xac430000|lo;
    w[0x2078/4] = 0x24030003;
}

static void music_fixture(uint32_t *w, uint32_t base)
{
    static const uint32_t prefix[] = {0x27bdff70,0xafb3007c,0x00809821,0x03a02021,
        0xafbf0080,0xafb20078,0x27b2000c,0xafb10074};
    uint32_t path = base+TAPE_MUSIC_ICON_PATH;
    memset(w, 0, TAPE_MUSIC_TEXT_SIZE);
    memcpy(w+TAPE_MUSIC_LOADER/4, prefix, sizeof prefix);
    w[TAPE_MUSIC_PATH_LUI/4] = 0x3c050000|((path+0x8000)>>16);
    w[TAPE_MUSIC_PATH_LUI/4+1] = 0x02202021;
    w[TAPE_MUSIC_STRING_CALL/4] = 0x0c000000|((base+TAPE_MUSIC_STRING_CTOR)>>2);
    w[TAPE_MUSIC_STRING_CALL/4+1] = 0x24a50000|(path&0xffff);
    w[TAPE_MUSIC_EPILOGUE/4] = 0x03e00008;
    w[TAPE_MUSIC_EPILOGUE/4+1] = 0x27bd0090;
    strcpy((char *)w+TAPE_MUSIC_ICON_PATH, tape_stock_icon_path);
}

int main(void)
{
    const uint32_t bases[] = {0x08880000, 0x09d00000, 0x0a800000, 0x0bf00000};
    const char *paths[] = {"ms0:/seplugins/viz_tape/viz_tape.prx", "ef0:/seplugins/viz_tape/viz_tape.prx"};
    uint32_t *viz = malloc(TAPE_TEXT_SIZE), *music = malloc(TAPE_MUSIC_TEXT_SIZE);
    unsigned char *before = malloc(TAPE_MUSIC_TEXT_SIZE);
    unsigned i, d, b;
    assert(viz && music && before);
    for (i = 0; i < sizeof bases/sizeof bases[0]; ++i) {
        uint32_t base = bases[i];
        visualizer_fixture(viz, base);
        assert(tape_set_user_memory(0x08800000, 0x01800000));
        assert(tape_profile_matches(viz, TAPE_TEXT_SIZE, base) == (i < 2));
        assert(tape_set_user_memory(0x08800000, 0x03800000));
        assert(tape_profile_matches(viz, TAPE_TEXT_SIZE, base));
        assert(tape_play_state_address(viz, TAPE_TEXT_SIZE, base) == base+TAPE_PLAY_STATE);
        viz[TAPE_VTABLE/4] ^= 4;
        assert(!tape_profile_matches(viz, TAPE_TEXT_SIZE, base));
        for (d = 0; d < 2; ++d) {
            assert(tape_paths_init(paths[d], strlen(paths[d])+1));
            music_fixture(music, base);
            memcpy(before, music, TAPE_MUSIC_TEXT_SIZE);
            assert(tape_set_user_memory(0x08800000, 0x01800000));
            if (i >= 2) {
                assert(!tape_icon_redirect(music, TAPE_MUSIC_TEXT_SIZE, base));
                assert(!memcmp(before, music, TAPE_MUSIC_TEXT_SIZE));
            }
            assert(tape_set_user_memory(0x08800000, 0x03800000));
            assert(tape_icon_redirect(music, TAPE_MUSIC_TEXT_SIZE, base));
            assert(!strcmp((char *)music+TAPE_MUSIC_ICON_PATH, TAPE_ICON_PATH));
            for (b = 0; b < TAPE_MUSIC_TEXT_SIZE; ++b) {
                if (b >= TAPE_MUSIC_ICON_PATH && b <= TAPE_MUSIC_ICON_PATH+strlen(tape_stock_icon_path)) continue;
                assert(((unsigned char *)music)[b] == before[b]);
            }
            assert(!tape_icon_redirect(music, TAPE_MUSIC_TEXT_SIZE, base));
            music_fixture(music, base);
            music[TAPE_MUSIC_LOADER/4] ^= 1;
            memcpy(before, music, TAPE_MUSIC_TEXT_SIZE);
            assert(!tape_icon_redirect(music, TAPE_MUSIC_TEXT_SIZE, base));
            assert(!memcmp(before, music, TAPE_MUSIC_TEXT_SIZE));
        }
    }
    free(before); free(music); free(viz);
    puts("Compatibility: low/high memory signature relocation, ms0/ef0 RAM redirect, unchanged surrounding bytes and mismatch rejection passed.");
    return 0;
}
