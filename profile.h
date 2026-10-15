#ifndef VIZ_TAPE_PROFILE_H
#define VIZ_TAPE_PROFILE_H
#include <stddef.h>
#include <stdint.h>
#define TAPE_TEXT_SIZE 0x14a48u
#define TAPE_VTABLE 0x148b8u
#define TAPE_VTABLE_SLOTS 15u
#define TAPE_DRAW_SLOT 0x148c4u
#define TAPE_ORIGINAL_DRAW 0x91ecu
#define TAPE_DATA_SIZE 0x68u
#define TAPE_BSS_SIZE 0x1958u
/* Native dispatch through vtable slot 3, and the fade load at effect+0x24. */
#define TAPE_DISPATCH 0x1f08u
#define TAPE_FADE_LOAD 0x9244u
/* Visualizer import stubs for the PAF drawing functions, with their NIDs. */
#define TAPE_STUB_DRAW 0x12a68u
#define TAPE_STUB_ENABLE 0x12a20u
#define TAPE_STUB_DISABLE 0x12b98u
#define TAPE_STUB_BLEND 0x129f8u
#define TAPE_NID_DRAW 0x7a56ca5au
#define TAPE_NID_ENABLE 0x704ea77eu
#define TAPE_NID_DISABLE 0xc53f5657u
#define TAPE_NID_BLEND 0x6627722du
/* Playback-state global in the visualizer's data, and its getter function. */
#define TAPE_PLAY_STATE 0x14ae0u
#define TAPE_STATE_GETTER 0x2114u
int tape_profile_matches(const void *text, size_t size, uint32_t load_address);
/* Returns the relocated playback-state address, or 0 if the getter differs. */
uint32_t tape_play_state_address(const void *text, size_t size, uint32_t load_address);
#endif
