#!/usr/bin/env python3
"""Converte um icone PNG de fundo branco numa imagem LVGL 9 (ARGB8888) em C.

So biblioteca padrao. Feito para o logo do header: o fundo branco vira
transparente (as bordas suavizadas continuam suaves no header escuro), a
imagem e recortada ao conteudo e reduzida para um quadrado de N px.

Uso:
  python tools/logo2c.py assets-local/ticktick-180.png --size 28 \
      --name logo_ticktick --out firmware/ticktick_screen/src/assets

Gera <out>/<name>.c e <out>/<name>.h. O logo do TickTick e marca registrada:
os arquivos gerados ficam fora do git (.gitignore). Sem eles, o firmware
compila normalmente e o header fica sem logo.
"""
import argparse
import os
import struct
import sys
import zlib

PNG_SIG = b"\x89PNG\r\n\x1a\n"


def _unfilter(raw, w, h, bpp):
    stride = w * bpp
    out = bytearray()
    prev = bytearray(stride)
    pos = 0
    for _ in range(h):
        ftype = raw[pos]
        line = bytearray(raw[pos + 1:pos + 1 + stride])
        pos += 1 + stride
        for i in range(stride):
            a = line[i - bpp] if i >= bpp else 0
            b = prev[i]
            c = prev[i - bpp] if i >= bpp else 0
            if ftype == 1:
                line[i] = (line[i] + a) & 0xFF
            elif ftype == 2:
                line[i] = (line[i] + b) & 0xFF
            elif ftype == 3:
                line[i] = (line[i] + ((a + b) >> 1)) & 0xFF
            elif ftype == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                pred = a if pa <= pb and pa <= pc else (b if pb <= pc else c)
                line[i] = (line[i] + pred) & 0xFF
            elif ftype != 0:
                raise ValueError(f"filtro PNG desconhecido: {ftype}")
        out += line
        prev = line
    return out


def read_png(data):
    """(w, h, [(r, g, b, a), ...]) de um PNG de 8 bits sem entrelacamento."""
    if data[:8] != PNG_SIG:
        raise ValueError("nao e PNG")
    pos = 8
    w = h = depth = ctype = interlace = None
    palette, trns, idat = [], b"", bytearray()
    while pos < len(data):
        n = struct.unpack(">I", data[pos:pos + 4])[0]
        kind = data[pos + 4:pos + 8]
        body = data[pos + 8:pos + 8 + n]
        pos += 12 + n
        if kind == b"IHDR":
            w, h, depth, ctype, _, _, interlace = struct.unpack(">IIBBBBB", body)
        elif kind == b"PLTE":
            palette = [tuple(body[i:i + 3]) for i in range(0, len(body), 3)]
        elif kind == b"tRNS":
            trns = body
        elif kind == b"IDAT":
            idat += body
        elif kind == b"IEND":
            break
    if depth != 8 or interlace:
        raise ValueError("so PNG de 8 bits por canal, sem entrelacamento")
    bpp = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}.get(ctype)
    if bpp is None:
        raise ValueError(f"tipo de cor PNG nao suportado: {ctype}")
    px = _unfilter(zlib.decompress(bytes(idat)), w, h, bpp)
    out = []
    for i in range(w * h):
        s = px[i * bpp:(i + 1) * bpp]
        if ctype == 6:
            out.append(tuple(s))
        elif ctype == 2:
            out.append((s[0], s[1], s[2], 255))
        elif ctype == 3:
            r, g, b = palette[s[0]]
            a = trns[s[0]] if s[0] < len(trns) else 255
            out.append((r, g, b, a))
        elif ctype == 0:
            out.append((s[0], s[0], s[0], 255))
        else:  # 4: cinza + alfa
            out.append((s[0], s[0], s[0], s[1]))
    return w, h, out


def _inks(pixels):
    """Cores "de tinta": frequentes (>= 1%) e longe do branco."""
    count = {}
    for r, g, b, a in pixels:
        if a == 255 and 255 - min(r, g, b) > 60:
            count[(r, g, b)] = count.get((r, g, b), 0) + 1
    floor = max(1, len(pixels) // 100)
    inks = [c for c, n in count.items() if n >= floor]
    return inks or [(0, 0, 0)]


def white_to_alpha(pixels):
    """Desfaz a mistura com o fundo branco: p = a*tinta + (1-a)*branco."""
    inks = _inks(pixels)
    out = []
    for r, g, b, a0 in pixels:
        best = None
        for ir, ig, ib in inks:
            d = ((255 - ir) ** 2 + (255 - ig) ** 2 + (255 - ib) ** 2) or 1
            a = ((255 - r) * (255 - ir) + (255 - g) * (255 - ig) + (255 - b) * (255 - ib)) / d
            a = min(1.0, max(0.0, a))
            er = (r - (a * ir + (1 - a) * 255)) ** 2 + (g - (a * ig + (1 - a) * 255)) ** 2 \
                + (b - (a * ib + (1 - a) * 255)) ** 2
            if best is None or er < best[0]:
                best = (er, a, (ir, ig, ib))
        _, a, ink = best
        alpha = round(a * a0)
        out.append((ink[0], ink[1], ink[2], alpha) if alpha else (0, 0, 0, 0))
    return out


def crop_square(w, h, pixels, min_alpha=8):
    """Recorta ao conteudo visivel e centraliza num quadrado transparente."""
    xs = [i % w for i, p in enumerate(pixels) if p[3] >= min_alpha]
    ys = [i // w for i, p in enumerate(pixels) if p[3] >= min_alpha]
    if not xs:
        raise ValueError("imagem sem conteudo visivel")
    x0, x1, y0, y1 = min(xs), max(xs), min(ys), max(ys)
    side = max(x1 - x0, y1 - y0) + 1
    ox = x0 - (side - (x1 - x0 + 1)) // 2
    oy = y0 - (side - (y1 - y0 + 1)) // 2
    out = []
    for y in range(oy, oy + side):
        for x in range(ox, ox + side):
            out.append(pixels[y * w + x] if 0 <= x < w and 0 <= y < h else (0, 0, 0, 0))
    return side, out


def downscale(side, pixels, size):
    """Media por area em alfa pre-multiplicado (bordas sem halo escuro)."""
    out = []
    for ty in range(size):
        y0, y1 = ty * side / size, (ty + 1) * side / size
        for tx in range(size):
            x0, x1 = tx * side / size, (tx + 1) * side / size
            acc = [0.0, 0.0, 0.0, 0.0]
            area = 0.0
            for sy in range(int(y0), min(side, int(y1) + 1)):
                wy = min(y1, sy + 1) - max(y0, sy)
                if wy <= 0:
                    continue
                for sx in range(int(x0), min(side, int(x1) + 1)):
                    wx = min(x1, sx + 1) - max(x0, sx)
                    if wx <= 0:
                        continue
                    r, g, b, a = pixels[sy * side + sx]
                    k = wx * wy
                    acc[0] += r * a * k
                    acc[1] += g * a * k
                    acc[2] += b * a * k
                    acc[3] += a * k
                    area += k
            a = acc[3] / area
            if acc[3] <= 0:
                out.append((0, 0, 0, 0))
            else:
                out.append((round(acc[0] / acc[3]), round(acc[1] / acc[3]),
                            round(acc[2] / acc[3]), round(a)))
    return out


def to_c(name, size, pixels):
    """(.h, .c) com um lv_image_dsc_t ARGB8888 (bytes B, G, R, A)."""
    data = bytearray()
    for r, g, b, a in pixels:
        data += bytes((b, g, r, a))
    rows = [", ".join(f"0x{v:02x}" for v in data[i:i + 16]) for i in range(0, len(data), 16)]
    guard = f"ASSETS_{name.upper()}_H"
    h = (f"// Gerado por tools/logo2c.py. Nao versionar (marca de terceiros).\n"
         f"#ifndef {guard}\n#define {guard}\n\n#include <lvgl.h>\n\n"
         f"extern const lv_image_dsc_t {name};\n\n#endif // {guard}\n")
    c = (f"// Gerado por tools/logo2c.py. Nao versionar (marca de terceiros).\n"
         f"#include \"{name}.h\"\n\n"
         f"static const uint8_t {name}_data[] = {{\n    " + ",\n    ".join(rows) + "\n};\n\n"
         f"const lv_image_dsc_t {name} = {{\n"
         f"    .header = {{\n"
         f"        .magic = LV_IMAGE_HEADER_MAGIC,\n"
         f"        .cf = LV_COLOR_FORMAT_ARGB8888,\n"
         f"        .flags = 0,\n"
         f"        .w = {size},\n"
         f"        .h = {size},\n"
         f"        .stride = {size * 4},\n"
         f"    }},\n"
         f"    .data_size = sizeof({name}_data),\n"
         f"    .data = {name}_data,\n"
         f"}};\n")
    return h, c


def main(argv=None):
    ap = argparse.ArgumentParser(description="icone PNG de fundo branco -> imagem LVGL em C")
    ap.add_argument("png")
    ap.add_argument("--size", type=int, default=28, help="lado do quadrado final, em px")
    ap.add_argument("--name", default="logo_ticktick")
    ap.add_argument("--out", default=".")
    args = ap.parse_args(argv)
    with open(args.png, "rb") as f:
        w, h, px = read_png(f.read())
    side, sq = crop_square(w, h, white_to_alpha(px))
    small = downscale(side, sq, args.size)
    hdr, src = to_c(args.name, args.size, small)
    os.makedirs(args.out, exist_ok=True)
    for ext, text in ((".h", hdr), (".c", src)):
        with open(os.path.join(args.out, args.name + ext), "w", encoding="utf-8", newline="\n") as f:
            f.write(text)
    print(f"{args.out}/{args.name}.c  {args.size}x{args.size} ARGB8888")
    return 0


if __name__ == "__main__":
    sys.exit(main())
