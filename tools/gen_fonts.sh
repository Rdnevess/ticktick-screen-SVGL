#!/usr/bin/env bash
#
# Gera as fontes da UI com acentuacao (Latin-1) a partir da Montserrat, e a
# fonte de digitos do relogio (Plano C).
#
# As Montserrat de fabrica do LVGL so tem ASCII + grau + bullet, e os titulos
# das tarefas vem do TickTick com acento. Sem isto, "Revisão" aparece quebrado.
#
# O TTF vem da propria lib lvgl (mesmo arquivo com que a LVGL gera as fontes de
# fabrica, entao o resultado e visualmente identico). Sobrescreva com:
#   MONTSERRAT_TTF=/caminho/Montserrat-Medium.ttf ./tools/gen_fonts.sh
#
# Requer Node (usa npx lv_font_conv, versao travada).
#
# A versao 1.5.3 e o piso: a 1.5.2 emite `#if LV_VERSION_CHECK(8, 0, 0)` para
# decidir se o descritor e const, e em LVGL 9 esse macro testa major EXATAMENTE
# 8 — da falso, o arquivo define a fonte sem const e o build quebra com
# "conflicting type qualifiers" contra o extern const de LV_FONT_DECLARE.
# Nao baixar a versao.
#
# --no-compress e OBRIGATORIO, nao otimizacao. Por padrao o lv_font_conv emite
# glifos comprimidos (.bitmap_format = 1), e o LVGL 9.2 vem com
# LV_USE_FONT_COMPRESSED 0 — sem descompressor. O resultado nao e erro de build:
# a fonte carrega e TODO caractere desenha como retangulo de glifo ausente.
#
# O lv_font_conv grava a linha de comando (com caminhos absolutos) no cabecalho
# do .c; ela e trocada por uma linha fixa para o arquivo nao mudar de maquina
# para maquina.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT="$HERE/../firmware/ticktick_screen/src/assets"
LV_FONT_CONV_VERSION="1.5.3"
SIZES=(12 14 18 24 28 36 48)

ttf="${MONTSERRAT_TTF:-}"
if [ -z "$ttf" ]; then
  userdir="$(arduino-cli config get directories.user 2>/dev/null || true)"
  ttf="$userdir/libraries/lvgl/scripts/built_in_font/Montserrat-Medium.ttf"
fi
if [ ! -f "$ttf" ]; then
  echo "erro: Montserrat-Medium.ttf nao encontrado em:" >&2
  echo "      $ttf" >&2
  echo "      instale a lib lvgl 9.2.2 ou defina MONTSERRAT_TTF=<caminho>" >&2
  exit 1
fi
echo "==> fonte: $ttf"

# Intervalos do texto:
#   0x20-0x7F  ASCII imprimivel
#   0xA0-0xFF  Latin-1 Supplement (a-til, c-cedilha, grau...)
#   0x2022 bullet · 0x2014 travessao · 0x2026 elipse · 0x201C-0x201D aspas curvas
TEXT_RANGES=(-r 0x20-0x7F -r 0xA0-0xFF -r 0x2022 -r 0x2014 -r 0x2026 -r 0x201C-0x201D)
# Relogio: espaco, hifen (do "--:--"), digitos e dois pontos.
CLOCK_RANGES=(-r 0x20 -r 0x2D -r 0x30-0x3A)

mkdir -p "$OUT"
total=0

gen() { # gen <nome> <tamanho> <intervalos...>
  local name="$1" size="$2"
  shift 2
  local out="$OUT/$name.c"
  npx --yes "lv_font_conv@$LV_FONT_CONV_VERSION" \
    --font "$ttf" --size "$size" --bpp 4 --format lvgl --no-compress \
    --lv-include lvgl.h -o "$out" "$@"
  # Guarda contra regressao: glifo comprimido vira tela de retangulos.
  if ! grep -q '\.bitmap_format = 0,' "$out"; then
    echo "erro: $out saiu com glifos comprimidos (.bitmap_format != 0)." >&2
    exit 1
  fi
  # Cabecalho sem caminho da maquina de quem gerou.
  sed "s|^ \* Opts: .*$| * Opts: gerado por tools/gen_fonts.sh (lv_font_conv $LV_FONT_CONV_VERSION)|" \
    "$out" > "$out.tmp" && mv "$out.tmp" "$out"
  local bytes
  bytes="$(grep -oE '0x[0-9a-fA-F]{2}' "$out" | wc -l | tr -d ' ')"
  total=$((total + bytes))
  printf "    %-15s %7s bytes de glifo\n" "$name" "$bytes"
}

for s in "${SIZES[@]}"; do
  gen "font_pt_$s" "$s" "${TEXT_RANGES[@]}"
done
gen "font_clock_120" 120 "${CLOCK_RANGES[@]}"

echo "==> pronto: $OUT  (~$((total / 1024)) KB de flash no total)"
