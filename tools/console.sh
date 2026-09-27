#!/usr/bin/env bash
#
# Manda comandos ao console serial do aparelho e mostra o que ele responde.
# Abrir a porta REINICIA a placa (USB CDC): o script espera o boot antes de
# digitar. O estado entre execucoes e so o que estiver na NVS.
#
# Uso:
#   tools/console.sh "help"
#   tools/console.sh -w 20 "pair $(cat ~/tt-blob.txt)" "projects"
#     -p PORTA   porta serial (padrao: detectada)
#     -w SEG     espera do boot antes do primeiro comando (padrao 8)
#     -c SEG     espera depois de cada comando (padrao 4)
set -euo pipefail

port=""
wait_boot=8
per_cmd=4
while getopts "p:w:c:" o; do
  case "$o" in
    p) port="$OPTARG" ;;
    w) wait_boot="$OPTARG" ;;
    c) per_cmd="$OPTARG" ;;
    *) exit 1 ;;
  esac
done
shift $((OPTIND - 1))

if [ -z "$port" ]; then
  port="$(arduino-cli board list 2>/dev/null | awk '$0 ~ /esp32:esp32/ { print $1; exit }')"
fi
if [ -z "$port" ]; then
  echo "erro: placa nao encontrada; passe a porta com -p" >&2
  exit 1
fi

total=$((wait_boot + per_cmd * $# + 2))
{
  sleep "$wait_boot"
  for c in "$@"; do
    printf '%s\n' "$c"
    sleep "$per_cmd"
  done
} | timeout "$total" arduino-cli monitor -p "$port" -c baudrate=115200 --quiet || true
