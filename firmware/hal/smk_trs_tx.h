/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stdint.h>
/* SMK-37 Pro TRS MIDI OUT: UART1 TX on PC7. Reproduces the stock Pro016 setup
 * (0x2006170 mux nibble 20..23 = 1, PC7 output; 0x202daf4 writes TXADR/TXCNT; the
 * 0x202de54 ISR tests pending bit 15, cleared with bit 13). Poll DMA completion
 * instead of enabling the TX interrupt. Verified in the felucca-smk fork (Test21:
 * output into an FM-1, chords/sustain/pitch bend). Other port C bits (LCD CS PC8,
 * audio gate PC6, light gate PC10) are preserved: only bit 7 is touched. */
#ifndef SMK_TRS_REG
#define SMK_TRS_REG(a) (*(volatile uint32_t *)(uintptr_t)(a))
#endif
#ifndef SMK_TRS_SYNC
#define SMK_TRS_SYNC() __asm__ volatile("csync" ::: "memory")
#endif
static void smk_trs_uart_start(void)
{
    SMK_TRS_REG(0x12100) = 0x3400;
    SMK_TRS_REG(0x12104) = 0;
    SMK_TRS_REG(0x10010) = (SMK_TRS_REG(0x10010) & ~(3u << 10)) | (1u << 10);   /* PLL48M */
    SMK_TRS_REG(0x51028) = (SMK_TRS_REG(0x51028) & ~(0xf00000u | 0x80u)) | 0x100000u;
    SMK_TRS_REG(0x50090) |= 1u << 7;   /* PC7 pull-up */
    SMK_TRS_REG(0x50094) |= 1u << 7;   /* PC7 pull-down (stock setup) */
    SMK_TRS_REG(0x50080) &= ~(1u << 7);
    SMK_TRS_REG(0x50088) &= ~(1u << 7);
    SMK_TRS_REG(0x5008c) |= 1u << 7;
    SMK_TRS_REG(0x12108) = 48000000u / 31250u / 4u - 1u;
    SMK_TRS_SYNC();
    SMK_TRS_REG(0x12100) = 0x3421u;    /* enable, no RX, no IRQ */
}
static int smk_trs_uart_done(void) { return !!(SMK_TRS_REG(0x12100) & 0x8000u); }
static void smk_trs_uart_send(const uint8_t *bytes, unsigned n)
{
    SMK_TRS_REG(0x12100) |= 0x2000u;
    SMK_TRS_REG(0x12114) = (uint32_t)(uintptr_t)bytes;
    SMK_TRS_SYNC();
    SMK_TRS_REG(0x12118) = n;
}
static void smk_trs_uart_stop(void)
{
    SMK_TRS_REG(0x12100) = 0x3400;
    SMK_TRS_REG(0x50080) |= 1u << 7;   /* idle high */
}
