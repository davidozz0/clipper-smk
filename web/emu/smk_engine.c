/* SPDX-License-Identifier: GPL-3.0-only
 * SMK-37 Pro browser runtime for THIS port: ChanceTheMaker's browser ABI
 * (Felucca SMK, web/audio/smk-engine.c, GPL-3.0) implemented over this port's engine
 * (web/emu/felucca_smk_web.c: the firmware's own sources + the SMK panel simulator).
 *
 * With this, their pages need no adapter at all: web/audio/smk-worklet.js,
 * web/audio/smk-browser.js, web/simulator-page.js and web/smk-simulator.js run unchanged.
 *
 * The ABI:
 *   synth_init()
 *   synth_render()          -> byte offset of 32 stereo frames of int32 (±32767), CTL = 32
 *   synth_midi(st, d1, d2)
 *   fw_event(kind, id, value)  kind: 0 buttons (18, by id) · 1 encoders (± steps)
 *                                   2 keys (37, velocity) · 3 pads (16, velocity)
 *                                   4 faders (0..1023) · 5 wheels (0 pitch -8192..8191, 1 mod 0..127)
 *                                   6 sustain
 *   fw_release()
 *   fw_screen()             -> 240 x 240 RGB565, high byte first (as the panel gets it)
 *   fw_status()             -> uint32[16]: [0] the 19 button LEDs, [1..2] keys held, [3] pads held,
 *                                          [6] playing, [11] master, [12..15] the four faders
 *   fw_request()/fw_reply()/fw_command()/fw_changed()   no SysEx/editor bridge here
 *   fw_snapshot()/fw_snapshot_size()/fw_restore()       the browser's saved session = the
 *                                                       emulated flash's storage area
 */
#include "clipper_smk_web.c"

#define API __attribute__((used, visibility("default")))
#define SMK_SNAP_OFF 0x9D000u                       /* projects, presets, autosave, settings (SMK store) */
#define SMK_SNAP_LEN (0x100000u - SMK_SNAP_OFF)

static uint32_t status[16];
static uint32_t btn_mask;
static uint64_t held_keys;
static uint32_t held_pads;
static uint16_t faders[4];                          /* 0..1023, as the page sends them */
static int32_t out32[32 * 2];
static uint8_t req[2048];

API void synth_init(void)
{
    web_nor_erase();
    web_boot();
    btn_mask = 0;
    held_keys = 0;
    held_pads = 0;
    memset(faders, 0, sizeof faders);
}

API int32_t *synth_render(void)                     /* CTL = 32 frames, as the worklet reads them */
{
    uint32_t i;
    web_render(32u);
    for (i = 0; i < 32u; i++) {
        int32_t l = (int32_t)(out_l[i] * 32768.0f), r = (int32_t)(out_r[i] * 32768.0f);
        out32[2u * i] = l > 32767 ? 32767 : l < -32767 ? -32767 : l;
        out32[2u * i + 1u] = r > 32767 ? 32767 : r < -32767 ? -32767 : r;
    }
    return out32;
}

API void synth_midi(unsigned st, unsigned d1, unsigned d2)
{
    if (st >= 0x80u && st < 0xF0u && d1 < 128u && d2 < 128u)
        web_midi(st, d1, d2);
}

API void fw_event(unsigned kind, unsigned id, int value)
{
    switch (kind) {
    case 0:                                         /* buttons, by the SMK's own id (0..17) */
        if (id < 18u) {
            if (value)
                btn_mask |= 1u << id;
            else
                btn_mask &= ~(1u << id);
            web_buttons(btn_mask);
        }
        break;
    case 1:                                         /* encoders: ± detents */
        if (id < 8u && value) {
            int n = value < 0 ? -value : value;
            if (n > 32)
                n = 32;
            web_enc_raw(id, value < 0 ? -n : n);
        }
        break;
    case 2:                                         /* keys 0..36 with velocity */
        if (id < 37u) {
            value = value < 0 ? 0 : value > 127 ? 127 : value;
            if (value)
                held_keys |= (uint64_t)1 << id;
            else
                held_keys &= ~((uint64_t)1 << id);
            web_key(id, (uint32_t)value);
        }
        break;
    case 3:                                         /* pads 0..15 with velocity */
        if (id < 16u) {
            value = value < 0 ? 0 : value > 127 ? 127 : value;
            if (value)
                held_pads |= 1u << id;
            else
                held_pads &= ~(1u << id);
            web_pad(id, (uint32_t)value);
        }
        break;
    case 4:                                         /* faders 0..1023 */
        if (id < 4u) {
            value = value < 0 ? 0 : value > 1023 ? 1023 : value;
            faders[id] = (uint16_t)value;
            web_fader(id, (uint32_t)value >> 3);
        }
        break;
    case 5:                                         /* wheels */
        if (!id) {
            int32_t v = value < -8192 ? -8192 : value > 8191 ? 8191 : value;
            int32_t raw = 512 + (int32_t)((int64_t)v * 512 / 8192);
            web_wheel(0, (uint32_t)(raw < 0 ? 0 : raw > 1023 ? 1023 : raw));
        } else {
            int32_t v = value < 0 ? 0 : value > 127 ? 127 : value;
            web_wheel(1, (uint32_t)(615 - v));      /* the stock mod wheel is inverted: low raw = full */
        }
        break;
    case 6:                                         /* sustain */
        web_pedal(value ? 127u : 0u);
        break;
    }
}

API void fw_release(void)
{
    uint32_t i;
    for (i = 0; i < 37u; i++)
        if ((held_keys >> i) & 1u)
            web_key(i, 0);
    for (i = 0; i < 16u; i++)
        web_pad(i, 0);
    held_keys = 0;
    held_pads = 0;
    btn_mask = 0;
    web_buttons(0);
    web_pedal(0);
    web_wheel(0, 512u);                             /* the pitch wheel springs back to its centre */
}

API uint16_t *fw_screen(void) { return web_screen(); }

API uint32_t *fw_status(void)
{
    uint32_t b = web_lit_buttons(), i;
    /* their 19 LEDs: bits 0..15 = those buttons, 16 = PLAY, 17 = PLAY green, 18 = REC */
    status[0] = (b & 0xFFFFu) | ((b >> 16 & 1u) << 16) | ((b >> 16 & 1u) << 17) | ((b >> 17 & 1u) << 18);
    status[1] = (uint32_t)held_keys;
    status[2] = (uint32_t)(held_keys >> 32);
    status[3] = held_pads;
    status[4] = (uint32_t)song.master_q12;              /* (their panel ignores 4..10: the volume the mixer applies) */
    status[5] = 0u;
    status[6] = web_playing();
    status[7] = 0u;
    status[8] = 0u;
    status[9] = 0u;
    status[10] = 0u;
    status[11] = web_master_pot();
    for (i = 0; i < 4u; i++)
        status[12u + i] = faders[i];
    return status;
}

API uint8_t *fw_request(void) { return req; }
API uint8_t *fw_reply(void) { return req; }
API unsigned fw_command(unsigned n) { (void)n; return 0u; }    /* no editor/SysEx bridge: this is not their Studio */
API unsigned fw_changed(void) { return 0u; }

API uint8_t *fw_snapshot(void) { return nor + SMK_SNAP_OFF; }
API unsigned fw_snapshot_size(void) { return SMK_SNAP_LEN; }
API int fw_restore(void)
{
    web_boot();                                     /* the storage was replaced: boot the device on it */
    return 0;
}
