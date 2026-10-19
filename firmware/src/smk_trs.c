/* SPDX-License-Identifier: GPL-3.0-only
 * SMK-37 Pro: TRS MIDI OUT on the 3.5 mm jack (UART1 TX, pin PC7).
 *
 * Ported from the Felucca SMK-37 port (same board, same pin, the same stock setup:
 * see hal/smk_trs_tx.h). The instrument's out stream is one funnel (usb.c
 * midi_out_event): the notes the keys and the sequencer play, the wheels' bend,
 * the clock / start / stop of MENU > MIDI > CLOCK OUT. This file mirrors that
 * stream onto the jack, whether or not a computer is on the USB.
 *
 * NOT bridged: SysEx (the editor, the updater). A full ring drops the message,
 * never half of it.
 *
 * SMK_TRS_TARGET (felucca.c, the real firmware) brings the UART; every host build
 * (tests/hostsim.c) gets no-op stubs, so nothing there touches a register. */
#ifdef SMK_TRS_TARGET
#include "../hal/smk_trs_tx.h"

#define TRS_Q 128u
static struct {
    uint32_t event[TRS_Q];
    volatile uint32_t read, write;
    uint8_t bytes[3], length, position;
} trs_q;
static uint8_t trs_dma[16] __attribute__((aligned(16)));
static uint8_t trs_running, trs_busy;
static uint32_t trs_started;

/* a USB-MIDI event packet -> the bytes on the wire (0 = not bridged) */
static unsigned trs_decode(uint32_t pkt, uint8_t b[3])
{
    unsigned cin = pkt & 15u, s = (pkt >> 8) & 255u;
    b[0] = (uint8_t)s; b[1] = (uint8_t)(pkt >> 16); b[2] = (uint8_t)(pkt >> 24);
    if (cin >= 8u && cin <= 14u && (s >> 4) == cin && b[1] < 128u && ((cin == 12u || cin == 13u) || b[2] < 128u))
        return cin == 12u || cin == 13u ? 2u : 3u;
    if (cin == 15u && (s == 0xf8u || s == 0xfau || s == 0xfbu || s == 0xfcu || s == 0xfeu || s == 0xffu)) return 1u;
    if (cin == 3u && s == 0xf2u && b[1] < 128u && b[2] < 128u) return 3u;
    if (cin == 2u && (s == 0xf1u || s == 0xf3u) && b[1] < 128u) return 2u;
    if (cin == 5u && s == 0xf6u) return 1u;
    return 0u;                                   /* SysEx (editor / update) is never bridged */
}
static int trs_push(uint32_t pkt)
{
    uint8_t b[3];
    if (!trs_decode(pkt, b)) return 0;
    if (trs_q.write - trs_q.read >= TRS_Q) return -1;     /* full: drop, never half a message */
    trs_q.event[trs_q.write % TRS_Q] = pkt;
    __asm__ volatile("" ::: "memory");
    trs_q.write++;
    return 1;
}
static int trs_next(uint8_t *byte)
{
    if (trs_q.position == trs_q.length) {
        uint32_t p;
        trs_q.position = trs_q.length = 0;
        if (trs_q.read == trs_q.write) return 0;
        p = trs_q.event[trs_q.read % TRS_Q];
        trs_q.read++;
        trs_q.length = (uint8_t)trs_decode(p, trs_q.bytes);
        if (!trs_q.length) return 0;
    }
    *byte = trs_q.bytes[trs_q.position++];
    return 1;
}

/* producer: the audio ISR (usb.c midi_out_event) */
static void smk_trs_output(uint32_t pkt)
{
    if (trs_running) (void)trs_push(pkt);
}
/* boot: the jack on, before the audio starts */
static void smk_trs_start(void)
{
    memset(&trs_q, 0, sizeof trs_q);
    trs_busy = 0;
    smk_trs_uart_start();
    trs_running = 1;
}
/* 2 kHz (main.c TIMER5, beside usb_poll): drain the queue to the UART */
static void smk_trs_poll(void)
{
    uint32_t n = 0;
    if (!trs_running) return;
    if (trs_busy) {
        if (!smk_trs_uart_done()) {
            if (fm1_ticks() - trs_started > 24000000u) {      /* a stuck transfer: 1 s */
                smk_trs_uart_stop();
                trs_running = trs_busy = 0;
            }
            return;
        }
        trs_busy = 0;
    }
    while (n < sizeof trs_dma && trs_next(&trs_dma[n])) n++;
    if (n) { trs_started = fm1_ticks(); trs_busy = 1; smk_trs_uart_send(trs_dma, n); }
}
#else
/* host / emulator: the jack is not there, the out stream stays */
static void smk_trs_output(uint32_t pkt) { (void)pkt; }
static void smk_trs_start(void) {}
static void smk_trs_poll(void) {}
#endif
