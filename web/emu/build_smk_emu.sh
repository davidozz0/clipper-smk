#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Clipper (port SMK-37) nel browser: la pagina SMK di ChanceTheMaker resta quella,
# qui si costruisce solo il wasm che parla la sua ABI (web/emu/smk_engine.c).
#   sh web/emu/build_smk_emu.sh   ->  build/emu/smk-engine-clipper.wasm
# Poi: cp in ~/smk-37_cf/emu/page/audio/ (smk-engine-clipper.wasm + smk-build-clipper.json)
set -e
cd "$(dirname "$0")/../.."
if [ -f "$HOME/emsdk/emsdk_env.sh" ]; then . "$HOME/emsdk/emsdk_env.sh" >/dev/null 2>&1 || true; fi
command -v emcc >/dev/null 2>&1 || { echo "emu: emcc non trovato (. ~/emsdk/emsdk_env.sh)"; exit 1; }
[ -f build/gen/felucca_tables.h ] || { echo "emu: esegui prima ./build.sh (build/gen)"; exit 1; }
EXPORTS="synth_init synth_midi synth_render fw_event fw_release fw_screen fw_status fw_request fw_reply fw_command fw_changed fw_snapshot fw_snapshot_size fw_restore"
WEXPORTS=""
for e in $EXPORTS; do WEXPORTS="$WEXPORTS -Wl,--export=$e"; done
mkdir -p build/emu
# shellcheck disable=SC2086
emcc ${EMU_OPT:--O2} -ffp-contract=off -std=gnu99 -w -DBOARD_SMK37=1 -Ibuild/gen -Ifirmware/src $WEXPORTS \
    --no-entry -sSTANDALONE_WASM -sSTACK_SIZE=1048576 -sINITIAL_MEMORY=33554432 -sFILESYSTEM=0 \
    -o build/emu/smk-engine-clipper.wasm web/emu/smk_engine.c
echo "emu: build/emu/smk-engine-clipper.wasm ($(wc -c < build/emu/smk-engine-clipper.wasm | tr -d ' ') B)"
