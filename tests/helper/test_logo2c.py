"""Testes do conversor do logo. Rodar: python -m unittest discover -s tests/helper -v"""
import os
import struct
import sys
import tempfile
import unittest
import zlib

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "tools"))
import logo2c  # noqa: E402

WHITE = (255, 255, 255, 255)
BLUE = (71, 114, 250, 255)


def png_rgb(w, h, pixels):
    """PNG RGB de 8 bits, filtro 0 em todas as linhas."""
    raw = bytearray()
    for y in range(h):
        raw.append(0)
        for r, g, b, _ in pixels[y * w:(y + 1) * w]:
            raw += bytes((r, g, b))

    def chunk(kind, body):
        c = kind + body
        return struct.pack(">I", len(body)) + c + struct.pack(">I", zlib.crc32(c) & 0xFFFFFFFF)

    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(bytes(raw))) + chunk(b"IEND", b""))


def square_on_white(w, h, x0, y0, side, color):
    px = [WHITE] * (w * h)
    for y in range(y0, y0 + side):
        for x in range(x0, x0 + side):
            px[y * w + x] = color
    return px


class TestLogo(unittest.TestCase):
    def test_le_png_rgb(self):
        px = square_on_white(4, 3, 1, 1, 1, BLUE)
        w, h, out = logo2c.read_png(png_rgb(4, 3, px))
        self.assertEqual((w, h), (4, 3))
        self.assertEqual(out, px)

    def test_branco_vira_transparente_e_tinta_fica_opaca(self):
        px = square_on_white(20, 20, 5, 5, 10, BLUE)
        out = logo2c.white_to_alpha(px)
        self.assertEqual(out[0][3], 0)
        self.assertEqual(out[10 * 20 + 10], BLUE)

    def test_borda_misturada_vira_meio_transparente(self):
        px = square_on_white(20, 20, 5, 5, 10, BLUE)
        half = tuple(round((c + 255) / 2) for c in BLUE[:3]) + (255,)
        px[0] = half
        a = logo2c.white_to_alpha(px)[0]
        self.assertEqual(a[:3], BLUE[:3])
        self.assertAlmostEqual(a[3], 128, delta=2)

    def test_recorte_centraliza_num_quadrado(self):
        px = logo2c.white_to_alpha(square_on_white(20, 10, 2, 3, 4, BLUE))
        side, sq = logo2c.crop_square(20, 10, px)
        self.assertEqual(side, 4)
        self.assertTrue(all(p[3] == 255 for p in sq))

    def test_reducao_faz_media_da_area(self):
        px = [(0, 0, 255, 255), (0, 0, 0, 0), (0, 0, 0, 0), (0, 0, 255, 255)]
        self.assertEqual(logo2c.downscale(2, px, 1), [(0, 0, 255, 128)])

    def test_c_tem_descritor_argb8888(self):
        h, c = logo2c.to_c("logo_x", 1, [(1, 2, 3, 4)])
        self.assertIn("extern const lv_image_dsc_t logo_x;", h)
        self.assertIn("LV_COLOR_FORMAT_ARGB8888", c)
        self.assertIn(".stride = 4,", c)
        self.assertIn("0x03, 0x02, 0x01, 0x04", c)  # B, G, R, A

    def test_main_gera_h_e_c(self):
        with tempfile.TemporaryDirectory() as d:
            src = os.path.join(d, "in.png")
            with open(src, "wb") as f:
                f.write(png_rgb(20, 20, square_on_white(20, 20, 5, 5, 10, BLUE)))
            self.assertEqual(logo2c.main([src, "--size", "4", "--name", "logo_t", "--out", d]), 0)
            self.assertTrue(os.path.exists(os.path.join(d, "logo_t.c")))
            self.assertTrue(os.path.exists(os.path.join(d, "logo_t.h")))


if __name__ == "__main__":
    unittest.main()
