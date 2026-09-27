#!/usr/bin/env python3
"""Converte a saida do comando `shot` do console em PNG (so biblioteca padrao).

Uso:
  tools/console.sh -w 25 -c 8 "pin <seu PIN>" "shot foco" > log.txt
  python tools/shot2png.py log.txt --out docs/images

Cada bloco `-----BEGIN SHOT <nome> <w> <h>-----` ... `-----END SHOT-----` vira
<out>/<nome>.png. O corpo e base64 de registros (contagem u16 LE, pixel RGB565
u16 LE), linha a linha.
"""
import argparse
import base64
import os
import re
import struct
import sys
import zlib

BEGIN = re.compile(r"-----BEGIN SHOT (\S+) (\d+) (\d+)-----")
END = "-----END SHOT-----"


def rgb565_to_rgb(p):
    r, g, b = (p >> 11) & 0x1F, (p >> 5) & 0x3F, p & 0x1F
    return (r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2)


def rle_decode(data):
    if len(data) % 4:
        raise ValueError("corpo com tamanho que nao e multiplo de 4")
    out = []
    for count, px in struct.iter_unpack("<HH", data):
        out.extend([px] * count)
    return out


def parse_blocks(text):
    """Lista de (nome, w, h, pixels) na ordem em que aparecem."""
    shots = []
    cur = None
    for line in text.splitlines():
        line = line.strip()
        m = BEGIN.search(line)
        if m:
            cur = (m.group(1), int(m.group(2)), int(m.group(3)), [])
            continue
        if cur is None:
            continue
        if line == END:
            name, w, h, parts = cur
            pixels = rle_decode(base64.b64decode("".join(parts)))
            if len(pixels) != w * h:
                raise ValueError(f"{name}: {len(pixels)} pixels, esperados {w * h}")
            shots.append((name, w, h, pixels))
            cur = None
        else:
            cur[3].append(line)
    return shots


def _chunk(kind, body):
    c = kind + body
    return struct.pack(">I", len(body)) + c + struct.pack(">I", zlib.crc32(c) & 0xFFFFFFFF)


def png_bytes(w, h, pixels):
    raw = bytearray()
    for y in range(h):
        raw.append(0)  # filtro "none"
        for p in pixels[y * w:(y + 1) * w]:
            raw.extend(rgb565_to_rgb(p))
    ihdr = struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)
    return (b"\x89PNG\r\n\x1a\n" + _chunk(b"IHDR", ihdr) +
            _chunk(b"IDAT", zlib.compress(bytes(raw), 9)) + _chunk(b"IEND", b""))


def main(argv=None):
    ap = argparse.ArgumentParser(description="saida do `shot` -> PNG")
    ap.add_argument("log", help="arquivo com a saida do console")
    ap.add_argument("--out", default=".", help="pasta de destino")
    args = ap.parse_args(argv)
    with open(args.log, encoding="utf-8", errors="replace") as f:
        shots = parse_blocks(f.read())
    if not shots:
        print("nenhum bloco SHOT encontrado", file=sys.stderr)
        return 1
    os.makedirs(args.out, exist_ok=True)
    for name, w, h, pixels in shots:
        path = os.path.join(args.out, name + ".png")
        with open(path, "wb") as f:
            f.write(png_bytes(w, h, pixels))
        print(f"{path}  {w}x{h}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
