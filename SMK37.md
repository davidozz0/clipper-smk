# Clipper for the M-VAVE SMK-37 Pro

This branch is the **unofficial SMK-37 Pro port** of [Clipper](https://github.com/isod89/clipper-fm1)
by **isod89**: the same synth (23 engines, 407 factory sounds), on the SMK-37 Pro
(JieLi WL82/AC791N, 1 MiB flash, JEDEC `0xEB6014`).

Built on **Clipper 1.1** (upstream `main`), with the port's changes on top.

## What the port changes

- **The panel**: 37 keys with velocity, 18 buttons, 8 encoders (K1 = master volume, K2 = page,
  K3 = engine, K4 = sound, K5…K8 = the four knobs), 16 RGB pads, 2 wheels.
- **TRS MIDI OUT** on the 3.5 mm jack (the FM-1's jack is an input): everything that goes out of
  USB, clock included.
- **Sound picker on the pads**: turning PRESETS shows 16 sounds on the pads for a few seconds;
  KNOB BANK pins them; a pad loads its sound.
- **Master volume overlay** on K1; K1 is an encoder and needs no analog smoothing.
- **Pad LEDs**: a pad lights only while held (one constant colour); the WS2812 reset was wrong
  (36 bytes for a 16-pad chain).
- **Sounds grouped by engine**: `BANK[]` is one contiguous block per engine; ALGORITHM (K3) walks
  the engines, PRESETS (K4) walks the whole list.
- **The wheels on MIDI OUT**: pitch bend (14 bit) and CC1, one event per change; the bend range is
  Clipper 1.1's own (`GLO > MIDI > BEND`, RPN 0).
- **Faders off** (our unit's fader 1 is broken), data family `CLIPPER`, package `clipper-smk37`.

## Firmware

A ready-to-flash package is in [`dist/clipper-smk37.fwsc`](dist/clipper-smk37.fwsc)
(identity `SMK-37 Pro_900`, app area `0x4000…0x9D000`). Write it with the JieLi/M-VAVE
updater or our tool (`smk_flash_tui.py`, in the work repository, not here).

The package is built from this branch; if you change the source, rebuild it (below) instead of
flashing a stale file.

## Build

```sh
BOARD=smk37 sh ./build.sh        # -> build/clipper-smk37.fwsc
sh ./build.sh                    # the FM-1 build (upstream)
```

The JieLi toolchain and the AC79 SDK are **not** included (they are not ours to redistribute):
`tools/get_toolchain.sh` fetches the toolchain; point `AC79_SDK` at your SDK checkout.
The host tests need no toolchain: `tests/run_tests.sh`.

## Data and safety

- Only the app area `0x4000…0x9D000` is written; the head/SPL, calibration, BT and key data are
  never touched.
- The store from `0x9D000` has its own data family: switching firmware clears it on the first boot.
- Back up the flash twice before writing, and verify the read-back.

## License and credits

GPL-3.0-only, as upstream (`LICENSE`, `LICENSING.md`). Clipper is by **isod89**; the SLOOP base
and the SMK-37 layer are by **Leo Kuroshita / @kurogedelic / Hügelton Instruments** and the
**Snoop37** contributors; the second core is from **X0X** (Charles Vestal) after Melodee (Kerem
Kilic); FM6 uses **msfa** (Apache-2.0); PHYS uses DaisySP/Rings (MIT); the font is Terminus (OFL);
the samples are CC0. See `ATTRIBUTION.txt` and `LICENSES/`.

The port is ours, unofficial, and not endorsed by M-VAVE. "Clipper" and the sound engine are the
upstream authors' work.
