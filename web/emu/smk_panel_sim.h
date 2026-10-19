/* SPDX-License-Identifier: GPL-3.0-only
 * SMK-37 Pro panel simulator for the browser / native build (web/emu).
 *
 * Same idea as web/emu/felucca_web.c (which simulates the FM-1's panel): it provides the
 * HAL interface the UI expects, driven from outside instead of from the registers.
 * The SMK's HAL is firmware/hal/smk_input.h: 37 velocity keys (18 buttons, 8 encoders,
 * 16 pads, 4 faders, 2 wheels) and one virtual MASTER pot (encoder 0).
 *
 * Included before hostsim.c (seq.c needs the types) and guarded by FM1_IN_EXTERN.
 */
#include <string.h>
#include <stdint.h>

/* ------------------------------------------------------------- constants --- */
#define SMK_NPAD 16u
#define SMK_NLED 19u
#define FM1_NKEY 37u
#define FM1_NBTN 18u
#define SMK_KEY_F3 5u             /* keybed key 5 = F3: keys 5..31 are the FM-1's 27 note keys (bits 0..26) */
#define SMK_TRIM_FLASH 0xC1000u
#ifndef SMK_FADER_DISABLE
#define SMK_FADER_DISABLE 0u
#endif
enum { SMK_BTN_KNOB_BANK = 0, SMK_BTN_FADER_BANK = 1, SMK_BTN_LEFT = 2, SMK_BTN_RIGHT = 3 };
enum { FM1_BTN_OCT_DOWN = 4, FM1_BTN_OCT_UP = 5 };
#define FM1_OCT_BOTH (1u << FM1_BTN_OCT_DOWN | 1u << FM1_BTN_OCT_UP)
enum { FM1_ADC_BATT = 3, FM1_ADC_MASTER = 4 };
#define SMK_ENC_MASTER 0u         /* K1, where the FM-1 has its MASTER pot (top left) */
#define FM1_NENC 8u               /* encoders K1..K8 (smk_input.h) */

/* the 16 white keys the pads play, and the pad <-> white key mapping (smk_input.h) */
static const uint8_t SMK_WHITE_NOTE[SMK_NPAD] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};
static volatile uint8_t smk_pad_grid;
static volatile uint8_t smk_pad_piano;
static const int8_t SMK_PIANO_NOTE[SMK_NPAD] = {7, 9, 11, 12, 14, 16, 18, 19, 8, 10, -1, 13, 15, 17, -1, 20};
/* the pad picker (ui_input.c): a hit goes to the UI, not to the white key */
static volatile uint8_t smk_pad_pick, smk_pad_hits;
static uint8_t smk_pad_take(void) { uint8_t h = smk_pad_hits; smk_pad_hits = 0; return h; }
static uint8_t smk_pad_note[SMK_NPAD];                 /* the white key a held pad pressed, +1 (0 = none) */
#define SMK_PAD_OF_WHITE(w) (smk_pad_grid ? (w) : (w) < 8u ? (w) + 8u : (w) - 8u)
#define SMK_WHITE_OF_PAD(p) (smk_pad_grid ? (p) : (p) < 8u ? (p) + 8u : (p) - 8u)

/* ----------------------------------------------------------------- state --- */
static struct {
    volatile uint32_t notes;         /* bit n = note key n (0 = F3 .. 26 = G5) */
    uint32_t knotes, pnotes;
    volatile uint32_t buttons;       /* bit i = button i, by the HAL's button id (0..17) */
    uint32_t pressed, released;
    volatile uint32_t notes_pressed;
    uint64_t keys;                   /* all 37 keybed keys */
    uint8_t  key_vel[FM1_NKEY];
    uint8_t  fader[4];
    uint16_t fad_raw[4];
    uint16_t wheel[2];               /* raw: [0] pitch (its rest is the centre), [1] mod (low = up, as stock) */
    uint8_t  pad_st[SMK_NPAD];       /* 0x80 = down (what ui_leds reads for the pad's own light) */
    uint8_t  pad_press[SMK_NPAD];    /* 0..127 while down */
    uint16_t pedal, batt;
    uint32_t frames;
    uint32_t adc_ok, adc_miss;
} fm1_in = { .wheel = {512u, 615u} };                    /* pitch centred, mod down */

static uint8_t fm1_led[SMK_NLED], fm1_led_dim[SMK_NLED], fm1_led_breath[SMK_NLED], fm1_led_mid[SMK_NLED];
/* the FM-1's per-key LEDs do not exist on the SMK: a "key LED" is the button LED of that id */
static void fm1_led_key(uint32_t id, int on)
{
    if (id < SMK_NLED)
        fm1_led[id] = (uint8_t)(on != 0);
}
static uint32_t web_dim_lo;
static void fm1_led_dim_level(uint32_t lo) { web_dim_lo = lo; }

/* MASTER pot (smk_input.h: 0..1023, 24 a detent) */
static int32_t smk_master = 724;

/* the outer 10 keys (keybed keys 0..4 and 32..36), as seq.c ext_keys_block takes them */
static volatile uint16_t smk_extq[64];
static volatile uint32_t smk_extq_h, smk_extq_t;
static void smk_ext_push(uint32_t k, uint32_t v)
{
    if (smk_extq_t - smk_extq_h >= 64u)
        return;
    smk_extq[smk_extq_t & 63u] = (uint16_t)((k << 8) | (v & 0x7Fu));
    smk_extq_t++;
}
int smk_extkey_take(void)                                /* (key << 8) | velocity, or -1 */
{
    int32_t e;
    if (smk_extq_h == smk_extq_t)
        return -1;
    e = (int32_t)smk_extq[smk_extq_h & 63u];             /* read the HEAD, advance the HEAD */
    smk_extq_h++;                                        /* (the tail only grows in smk_ext_push) */
    return e;
}

/* ------------------------------------------------------------------ input --- */
static int32_t web_enc_steps[8];
/* one keybed key (0..36) down (vel 1..127) or up (0) */
static void smk_key_set(uint32_t k, uint32_t vel)
{
    uint64_t bit;
    if (k >= FM1_NKEY)
        return;
    bit = (uint64_t)1 << k;
    if (vel) {
        vel = vel > 127u ? 127u : vel;
        fm1_in.keys |= bit;
    } else {
        fm1_in.keys &= ~bit;
    }
    if (k >= SMK_KEY_F3 && k < SMK_KEY_F3 + 27u) {
        uint32_t nb = 1u << (k - SMK_KEY_F3);
        if (vel) {
            fm1_in.key_vel[k] = (uint8_t)vel;
            fm1_in.knotes |= nb;
            fm1_in.notes_pressed |= nb;
        } else {
            fm1_in.knotes &= ~nb;
        }
    } else {
        smk_ext_push(k, vel);                            /* the 10 keys past the FM-1's 27 (seq.c ext_keys_block) */
    }
    fm1_in.notes = fm1_in.knotes | fm1_in.pnotes;
}
static uint32_t fm1_input_edges(uint32_t *released)
{
    uint32_t p = fm1_in.pressed;
    if (released)
        *released = fm1_in.released;
    fm1_in.pressed = fm1_in.released = 0;
    return p;
}
static uint32_t fm1_input_note_edges(void) { uint32_t n = fm1_in.notes_pressed; fm1_in.notes_pressed = 0; return n; }
static int32_t fm1_enc_take(uint32_t e)
{
    int32_t s = web_enc_steps[e & 7u];
    web_enc_steps[e & 7u] = 0;
    return s;
}
static void fm1_input_init(void) {}
static void fm1_input_tick(void) {}
static void fm1_wdt_feed(void) {}
static void fm1_irq_off(void) {}
static void fm1_irq_on(void) {}
static int32_t fm1_adc_read(uint32_t ch)
{
    if (ch == FM1_ADC_MASTER) {                          /* smk_input.h: the FM-1's MASTER pot is knob K1 here */
        smk_master += fm1_enc_take(SMK_ENC_MASTER) * 24;
        smk_master = smk_master < 0 ? 0 : smk_master > 1023 ? 1023 : smk_master;
        return smk_master;
    }
    if (ch == FM1_ADC_BATT)
        return fm1_in.batt ? (int32_t)fm1_in.batt : -1;
    return -1;
}
static void smk_trim_defaults(void) {}
static void smk_trim_load(const void *p) { (void)p; }

/* ------------------------------------------------------------- pad LEDs --- */
static uint8_t padled_rgb[SMK_NPAD][3];
static uint32_t padled_hold_until;
static void padled_set(uint32_t pad, uint8_t r, uint8_t g, uint8_t b)
{
    if (pad < SMK_NPAD) {
        padled_rgb[pad][0] = r;
        padled_rgb[pad][1] = g;
        padled_rgb[pad][2] = b;
    }
}
static void padled_flush(void) {}
