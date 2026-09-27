#!/usr/bin/env bash
#
# Gera as capturas do README com o MODO DEMONSTRACAO: nenhuma tarefa real sai
# da placa. Uma sessao de console so (abrir e fechar a porta reinicia a placa).
#
# Uso:
#   PIN=<seu PIN> tools/capture_readme.sh             # placa com PIN
#   tools/capture_readme.sh                           # placa sem PIN
#   PIN=<seu PIN> BOOT_WAIT=45 tools/capture_readme.sh # boot mais lento (WiFi/SNTP/PIN)
#
# BOOT_WAIT (padrao 45): espera antes do primeiro comando. WiFi + SNTP + a
# tela de PIN podem levar mais de 25s; se o log mostrar "a tela de
# desbloqueio nao esta aberta" em vez de "pin: enviado", aumente e rode de novo.
#
# Saida: docs/images/<idioma>-<tela>.png
set -euo pipefail

BOOT_WAIT="${BOOT_WAIT:-45}"

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
OUT="$ROOT/docs/images"
LOG="$(mktemp)"
trap 'rm -f "$LOG"' EXIT

cmds=()
[ -n "${PIN:-}" ] && cmds+=("pin $PIN")
cmds+=("demo on")
for lang in en pt; do
  cmds+=("lang $lang" "demo off" "demo on") # tarefas de exemplo no idioma certo
  cmds+=("go foco" "shot $lang-focus")
  cmds+=("go hoje" "shot $lang-today")
  cmds+=("go status" "shot $lang-status")
  cmds+=("go relogio" "shot $lang-clock")
  cmds+=("go settings" "shot $lang-settings")
  cmds+=("go foco" "pomo start" "shot $lang-pomodoro" "pomo stop")
done
cmds+=("demo off" "lang pt")

"$HERE/console.sh" -w "$BOOT_WAIT" -c 10 "${cmds[@]}" > "$LOG"
python "$HERE/shot2png.py" "$LOG" --out "$OUT"
