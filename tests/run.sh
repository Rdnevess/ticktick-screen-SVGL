#!/usr/bin/env bash
#
# Roda os testes do nucleo no PC. Nenhuma placa envolvida.
#
# Compilador: g++ ou clang++ se existirem (Linux, macOS, contribuidores);
# senao MSVC via vcvars64.bat (Windows com Visual Studio Build Tools).
#
# ArduinoJson (necessario a partir do teste de payload) e procurado na pasta de
# libraries do arduino-cli; de para sobrescrever com ARDUINOJSON_SRC=<caminho>.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
CORE="$ROOT/firmware/ticktick_screen/src/core"
OUT="$HERE/.build"
mkdir -p "$OUT"

srcs=()
for f in "$HERE"/*.cpp "$CORE"/*.cpp; do [ -e "$f" ] && srcs+=("$f"); done
if [ ${#srcs[@]} -eq 0 ]; then echo "erro: nenhum fonte para compilar" >&2; exit 1; fi

# --- ArduinoJson (opcional ate a Task 8) ---
aj="${ARDUINOJSON_SRC:-}"
if [ -z "$aj" ]; then
  userdir="$(arduino-cli config get directories.user 2>/dev/null || true)"
  [ -n "$userdir" ] && aj="$userdir/libraries/ArduinoJson/src"
fi
incs=(-I"$HERE" -I"$CORE")
[ -n "$aj" ] && [ -d "$aj" ] && incs+=(-I"$aj")

if command -v g++ >/dev/null 2>&1;      then CXX=g++
elif command -v clang++ >/dev/null 2>&1; then CXX=clang++
else CXX=""
fi

if [ -n "$CXX" ]; then
  echo "==> $CXX"
  "$CXX" -std=c++17 -Wall -Wextra -O0 -g "${incs[@]}" -o "$OUT/tests" "${srcs[@]}"
  exec "$OUT/tests"
fi

# --- MSVC ---
vcvars=""
for base in "/c/Program Files/Microsoft Visual Studio" "/c/Program Files (x86)/Microsoft Visual Studio"; do
  for cand in "$base"/*/*/VC/Auxiliary/Build/vcvars64.bat; do
    [ -e "$cand" ] && vcvars="$cand" && break 2
  done
done
if [ -z "$vcvars" ]; then
  echo "erro: nenhum compilador C++ encontrado." >&2
  echo "       instale g++/clang++, ou o Visual Studio Build Tools (C++)." >&2
  exit 1
fi
echo "==> MSVC"

msvc_incs=(); for i in "${incs[@]}"; do msvc_incs+=("/I$(cygpath -w "${i#-I}")"); done
msvc_srcs=(); for s in "${srcs[@]}"; do msvc_srcs+=("$(cygpath -w "$s")"); done

bat="$OUT/build.bat"
{
  echo '@echo off'
  echo "call \"$(cygpath -w "$vcvars")\" >nul 2>&1"
  echo "cd /d \"$(cygpath -w "$OUT")\""
  printf 'cl /nologo /std:c++17 /EHsc /W3 /utf-8 /Zi'
  printf ' %s' "${msvc_incs[@]}"
  printf ' %s' "${msvc_srcs[@]}"
  printf ' /Fe:tests.exe /Fo:obj\\ /Fd:tests.pdb\n'
  echo 'exit /b %errorlevel%'
} > "$bat"
mkdir -p "$OUT/obj"

cmd.exe //c "$(cygpath -w "$bat")" || { echo "erro: compilacao falhou" >&2; exit 1; }
exec "$OUT/tests.exe"
