/* SPDX-License-Identifier: GPL-3.0-only */
/* The SMK-37's wheels on MIDI OUT: pitch bend (14 bit) and CC1, one event per change and nothing
 * while they rest. The bend range (RPN 0) is Clipper 1.1's, not this stream's. On the harness of
 * tests/hostsim.c (the MIDI out ring is visible there, as in tests/scale_test.c). */
#include <assert.h>
#define main hostsim_main
#include "hostsim.c"
#undef main

#define P(i) (midi_out_q[(i) % MQ])
static uint32_t st(uint32_t i) { return (P(i) >> 8) & 0xF0u; }
static uint32_t ch(uint32_t i) { return (P(i) >> 8) & 15u; }
static uint32_t d1(uint32_t i) { return (P(i) >> 16) & 127u; }
static uint32_t d2(uint32_t i) { return (P(i) >> 24) & 127u; }

int main(void)
{
    uint32_t base;

    usb.config = 1;
    mo_r = mo_w = 0;

    wheels_midi(-1, -1);                          /* nothing new: nothing goes out */
    wheels_midi_block();
    assert(mo_w == 0);

    wheels_midi(0x2000, 0);                       /* the centre, the mod at 0: nothing */
    wheels_midi_block();
    assert(mo_w == 0);

    base = mo_w;
    wheels_midi(0x0000, 0);                       /* full down */
    wheels_midi_block();
    assert(mo_w == base + 1 && st(base) == 0xE0u && d1(base) == 0u && d2(base) == 0u);
    assert(ch(base) == trk_midi_ch(song.sel));

    base = mo_w;
    wheels_midi(0x3FFF, 0);                       /* full up */
    wheels_midi_block();
    assert(mo_w == base + 1 && st(base) == 0xE0u && d1(base) == 127u && d2(base) == 127u);

    base = mo_w;
    wheels_midi(0x3FFE, 0);                       /* one step: under the 8/16384 dead band */
    wheels_midi_block();
    assert(mo_w == base);

    base = mo_w;
    wheels_midi(0x2000, 0);                       /* back to the centre: always sent */
    wheels_midi_block();
    assert(mo_w == base + 1 && st(base) == 0xE0u && d1(base) == 0u && d2(base) == 64u);

    base = mo_w;
    wheels_midi(0x2000, 0);                       /* resting: nothing */
    wheels_midi_block();
    assert(mo_w == base);

    base = mo_w;
    wheels_midi(0x2000, 100);                     /* the mod: CC1 */
    wheels_midi_block();
    assert(mo_w == base + 1 && st(base) == 0xB0u && d1(base) == 1u && d2(base) == 100u);

    base = mo_w;
    wheels_midi(0x2000, 100);                     /* the same value: nothing */
    wheels_midi_block();
    assert(mo_w == base);

    base = mo_w;
    wheels_midi(0x2000, 127);                     /* the top */
    wheels_midi_block();
    assert(mo_w == base + 1 && d2(base) == 127u);

    base = mo_w;
    wheels_midi(0x1000, 127);                     /* a new bend */
    wheels_midi_block();
    assert(mo_w == base + 1 && st(base) == 0xE0u);

    return 0;
}
