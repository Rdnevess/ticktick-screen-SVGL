#!/usr/bin/env bash
#
# Build / upload / monitor do bring-up (Guition JC4832W535, ESP32-S3).
#
# Uso:
#   ./build.sh                 # compila
#   ./build.sh upload          # compila + grava (porta detectada)
#   ./build.sh upload COM6     # compila + grava na porta indicada
#   ./build.sh monitor         # serial monitor a 115200
#
# PartitionScheme=huge_app (nao `custom`): esta pasta nao tem partitions.csv
# proprio, e `custom` faz o core abortar procurando um .csv inexistente.
#
# O -I vai TAMBEM em compiler.S.extra_flags: o core monta os .S da LVGL com
# essa flag, e sem ela a LVGL cai num lv_conf.h solto na pasta de libraries.
set -euo pipefail

SKETCH_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FQBN="esp32:esp32:esp32s3:PSRAM=opi,FlashSize=16M,PartitionScheme=huge_app,CDCOnBoot=cdc,USBMode=hwcdc,FlashMode=qio"
LVFLAGS="-DLV_CONF_INCLUDE_SIMPLE -I${SKETCH_DIR}"

detect_port() {
  # Primeira porta cujo core seja esp32:esp32 (ignora COM3/COM4 genericos)
  arduino-cli board list 2>/dev/null \
    | awk '$0 ~ /esp32:esp32/ { print $1; exit }'
}

cmd="${1:-build}"
port="${2:-$(detect_port)}"

if [ "$cmd" != "build" ] && [ -z "${port:-}" ]; then
  echo "erro: nenhuma placa esp32 encontrada. Ligue a placa ou passe a porta:" >&2
  echo "       ./build.sh $cmd COM6" >&2
  exit 1
fi

compile_args=(
  --fqbn "$FQBN"
  --build-property "compiler.cpp.extra_flags=$LVFLAGS"
  --build-property "compiler.c.extra_flags=$LVFLAGS"
  --build-property "compiler.S.extra_flags=$LVFLAGS"
)

case "$cmd" in
  monitor) exec arduino-cli monitor -p "$port" -c baudrate=115200 ;;
  build)   echo "==> compilando bring-up"; arduino-cli compile "${compile_args[@]}" "$SKETCH_DIR" ;;
  upload)  echo "==> compilando + gravando em $port"
           arduino-cli compile "${compile_args[@]}" --upload -p "$port" "$SKETCH_DIR" ;;
  *)       echo "comando desconhecido: $cmd (use: build | upload | monitor)" >&2; exit 1 ;;
esac
