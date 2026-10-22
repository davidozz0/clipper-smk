# clipper-smk — Clipper for the M-VAVE SMK-37 Pro

**Unofficial port** of [Clipper](https://github.com/isod89/clipper-fm1) (by **isod89**, GPL-3.0-only)
to the **M-VAVE SMK-37 Pro**. Same synth, same 23 engines and 407 factory sounds, on a keyboard
with 37 velocity keys, 16 RGB pads, 8 encoders, two wheels and a TRS MIDI jack.

Built on **Clipper 1.1** (`main`), with the port's changes on top — this branch, `smk37`.
A ready-to-flash package is in [`dist/clipper-smk37.fwsc`](dist/clipper-smk37.fwsc).

> Not affiliated with M-VAVE or with the Clipper authors. The synth is their work; the port and
> the changes listed below are ours.

## What the port changes

These are the differences from an upstream Clipper build. Each one exists for a reason; the
source of the first three is the SMK's hardware, the others are usability.

| change | why | where |
|---|---|---|
| **TRS MIDI OUT** on the 3.5 mm jack | the SMK's jack is an output (UART1 TX, PC7); the FM-1's is an input | `firmware/src/smk_trs.c`, `firmware/hal/smk_trs_tx.h`; mirrored in `usb.c` (`midi_out_event`) |
| **Faders off** | our unit's fader 1 is broken; `FADER BANK` stays without effect | `SMK_FADERS 0` in `firmware/hal/fm1_cc.h` |
| **The SMK panel** | 37 keys, 18 buttons, 8 encoders, 16 pads, 2 wheels | `firmware/hal/smk_input.h`, `firmware/src/panel.c` (`BOARD_SMK37`) |
| **Sound picker on the pads** | 407 sounds do not scroll well with one knob: turning PRESETS shows 16 sounds on the pads for 4 s, **KNOB BANK** pins them, a pad loads its sound | `firmware/src/ui_input.c` (`smk_pick_*`) |
| **Master volume overlay** | there was none: turning K1 shows `MASTER`, a bar and 0–100 in the top bar for ~1 s | `firmware/src/ui_draw.c` (`draw_head`), `ui_input.c` |
| **K1 without smoothing** | the smoothing was for the FM-1's analog master pot: with an encoder the level glided after the knob | `firmware/src/main.c` |
| **Pad LEDs** | they lit up when playing keys, and the next pad flashed: a pad lights only while held, one constant colour; the WS2812 reset was 36 bytes for a 16-pad chain (now 256) | `firmware/src/ui_input.c`, `lcd.c` |
| **Sounds grouped by engine** | the factory list was sorted by musical kind and scattered (194 blocks, 2.2 sounds each): now `BANK[]` is one contiguous block per engine, inside a block by kind | `firmware/src/ui.c` |
| **K3 / K4 behaviour** | with the list grouped, **ALGORITHM (K3)** walks the 23 engines and lands on the first sound of the next block; **PRESETS (K4)** walks the whole list and crosses engines; the browser's label is the engine (ANALOG, HYBRID, WAVE…) | `firmware/src/ui.c`, `ui_input.c`, `ui_draw.c` |
| **Wheels on MIDI OUT** | pitch and mod were applied to the sound only: now the pitch wheel sends a 14-bit **pitch bend** and the mod wheel **CC1**, one event per change (a dead band of 8/16384, the exact centre always sent) | `firmware/src/main.c` (`smk_wheels`), `seq.c` (`wheels_midi`, `wheels_midi_block`) |
| **Data family `CLIPPER`** | it must not read Felucca's or OberVave's store | `firmware/src/storage.c`, owner tag at `0xAF000` |
| **Package name and identity** | `clipper-smk37.fwsc`, identity `SMK-37 Pro_900` | `tools/build.py`, `build.sh`, `BUILDING.md` |

The bend range is **Clipper 1.1's own** (`GLO > MIDI > BEND`, AUTO or 1…24 semitones, RPN 0): the
wheels' stream carries no RPN of its own. `LOCAL OFF`, program change, CC out and EWI are 1.1
features and work here too.

### K3 and K4 in practice

```
preferiti → ANALOG 32 → DIGITAL 30 → PHASE 16 → … → JUNO 17 → VECTOR 18 → preset utente
```

K3 jumps from engine to engine (the message names it), K4 steps through the whole list. The
browser shows the engine on the left, the sound on the right. Favourites stay at the top (16 max);
the user presets at the end.

## Firmware

[`dist/clipper-smk37.fwsc`](dist/clipper-smk37.fwsc) is built from this branch:

| | |
|---|---|
| package | 651 226 B, identity `SMK-37 Pro_900`, sha256 `f1e96a07…` (the 2026-10-21 build, the one the web installer publishes) |
| app image | 586 272 B (fits the 619 520 B slot) |
| flash layout | app area `0x4000…0x9D000` only |

Write it with the M-VAVE/JieLi updater, with our tool (`tools/smk_flash_tui.py` in the work
repository), or from the browser (below). **Rules**: two identical full dumps before writing; only
the app area; read back and compare; keep the dumps. The head/SPL, calibration, BT and key data are
never touched.

### Web installer (no tools, no driver)

<https://davidozz0.github.io/clipper-smk/webapp/installer/> — open it in **Chrome or Edge** (Web Serial),
plug the SMK via USB, press connect and pick its port. The page checks the package's sha256, writes the
app area, verifies the identity the keyboard reports afterwards, and installs nothing on a device that
is not `SMK-37 Pro_9..`. The editor is at `/webapp/editor/`; the installer also restores an official
package you pick from disk (backup first — Clipper cannot open the stock presets).

The page is generated, not hand-written: `python3 web/make_site.py <pkg> <version> build/site --beta`
inlines `web/fm1ota.js`, `web/fm1pkg.js`, the metadata and the SMK-37 banner (`web/beta_banner.html`)
into `web/index_pkg.html`; the result goes to the `gh-pages` branch. The package it publishes is the one
passed to `make_site.py` — at the moment the 2026-10-21 build, sha256 `f1e96a07…`.

## Build

```sh
BOARD=smk37 sh ./build.sh        # -> build/clipper-smk37.fwsc
sh ./build.sh                    # the upstream FM-1 build
```

The JieLi toolchain and the AC79 SDK are **not** included (they are not ours to redistribute):
`tools/get_toolchain.sh` fetches the toolchain, and `AC79_SDK` points at your SDK checkout. The
host tests need no toolchain: `tests/run_tests.sh` (the Clipper UI test is the one to watch on a
port change).

## Status

- 8 voices, split across both cores (the second core renders half of a part's voices on 17 of the
  23 engines).
- Sounds, presets, projects and settings live in the `CLIPPER` data family; switching from another
  firmware clears the store from `0x9D000` on the first boot.
- Tested on the unit: keys and velocity, pads and picker, wheels, TRS out, flashing, saving.
- The host suite (`tests/run_tests.sh`) and the regression renders run on the MAC/PC side; see
  [`SMK37.md`](SMK37.md) for the port's notes.

## License and credits

**GPL-3.0-only**, as upstream: see [`LICENSE`](LICENSE) and [`LICENSING.md`](LICENSING.md). If you
distribute a binary, the source is here.

- Clipper: **isod89** (<https://github.com/isod89/clipper-fm1>).
- SLOOP base and the SMK-37 layer: **Leo Kuroshita / @kurogedelic / Hügelton Instruments** and the
  **Snoop37** contributors.
- Second core: **X0X** (Charles Vestal), after Melodee (Kerem Kilic).
- FM6: **msfa** (Apache-2.0); PHYS: DaisySP and Rings (MIT); font: Terminus (OFL); samples: CC0.
  See [`ATTRIBUTION.txt`](ATTRIBUTION.txt) and [`LICENSES/`](LICENSES/).

The original project's README is kept as [`README-UPSTREAM.md`](README-UPSTREAM.md).
