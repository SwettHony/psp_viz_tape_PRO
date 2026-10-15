#include "../profile.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    FILE *file;
    uint32_t *text = malloc(TAPE_TEXT_SIZE);
    uint32_t base = 0x09d3a900, saved;
    unsigned i;
    assert(argc == 2 && text);
    file = fopen(argv[1], "rb"); assert(file);
    assert(fread(text, 1, TAPE_TEXT_SIZE, file) == TAPE_TEXT_SIZE);
    fclose(file);
    assert(tape_profile_matches(text, TAPE_TEXT_SIZE, base));
    assert(!tape_profile_matches(text, TAPE_TEXT_SIZE-4, base));
    assert(!tape_profile_matches(text, TAPE_TEXT_SIZE, base+4));
    for (i = 0; i < TAPE_VTABLE_SLOTS; ++i) {
        text[TAPE_VTABLE/4+i] ^= 4;
        assert(!tape_profile_matches(text, TAPE_TEXT_SIZE, base));
        text[TAPE_VTABLE/4+i] ^= 4;
    }
    saved = text[TAPE_ORIGINAL_DRAW/4];
    text[TAPE_ORIGINAL_DRAW/4] ^= 1;
    assert(!tape_profile_matches(text, TAPE_TEXT_SIZE, base));
    text[TAPE_ORIGINAL_DRAW/4] = saved;
    text[0x1f18/4] ^= 4; /* Changed audio argument offset. */
    assert(!tape_profile_matches(text, TAPE_TEXT_SIZE, base));
    text[0x1f18/4] ^= 4;
    text[0x9244/4] ^= 4; /* Final effect's fade field must remain at +0x24. */
    assert(!tape_profile_matches(text, TAPE_TEXT_SIZE, base));
    text[0x9244/4] ^= 4;
    /* Playback state: captured layout found; any altered getter/store rejected. */
    assert(tape_play_state_address(text, TAPE_TEXT_SIZE, base) == base + TAPE_PLAY_STATE);
    assert(!tape_play_state_address(text, TAPE_TEXT_SIZE, base + 0x10000));
    {
        static const unsigned sites[] = {
            TAPE_STATE_GETTER, TAPE_STATE_GETTER+4, TAPE_STATE_GETTER+8,
            0x2064, 0x2068, 0x2070, 0x2078
        };
        for (i = 0; i < sizeof sites / sizeof sites[0]; ++i) {
            text[sites[i]/4] ^= 1;
            assert(!tape_play_state_address(text, TAPE_TEXT_SIZE, base));
            text[sites[i]/4] ^= 1;
        }
    }
    for (i = 0; i < TAPE_VTABLE_SLOTS; ++i) text[TAPE_VTABLE/4+i] -= 0x10000;
    assert(tape_profile_matches(text, TAPE_TEXT_SIZE, base-0x10000));
    free(text);
    puts("Profile: captured firmware and relocated layout pass; modified signatures/slots fail closed; seek state located.");
    return 0;
}
