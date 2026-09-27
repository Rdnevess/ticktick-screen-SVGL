"""Testes do conversor da captura de tela. Rodar: python -m unittest discover -s tests/helper -v"""
import base64
import os
import struct
import sys
import tempfile
import unittest
import zlib

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "tools"))
import shot2png  # noqa: E402


def encode(pixels):
    """Mesmo formato do firmware: (contagem u16 LE, pixel u16 LE) por corrida."""
    out = bytearray()
    i = 0
    while i < len(pixels):
        j = i
        while j < len(pixels) and pixels[j] == pixels[i] and j - i < 65535:
            j += 1
        out += struct.pack("<HH", j - i, pixels[i])
        i = j
    return out


def block(name, w, h, pixels):
    b64 = base64.b64encode(encode(pixels)).decode()
    lines = [b64[k:k + 76] for k in range(0, len(b64), 76)]
    return "\n".join([f"-----BEGIN SHOT {name} {w} {h}-----", *lines, "-----END SHOT-----"])


class TestShot(unittest.TestCase):
    def test_rgb565_para_rgb888_nos_extremos(self):
        self.assertEqual(shot2png.rgb565_to_rgb(0xFFFF), (255, 255, 255))
        self.assertEqual(shot2png.rgb565_to_rgb(0x0000), (0, 0, 0))
        self.assertEqual(shot2png.rgb565_to_rgb(0xF800), (255, 0, 0))
        self.assertEqual(shot2png.rgb565_to_rgb(0x07E0), (0, 255, 0))
        self.assertEqual(shot2png.rgb565_to_rgb(0x001F), (0, 0, 255))

    def test_rle_ida_e_volta(self):
        px = [1, 1, 1, 2, 3, 3] + [7] * 70000
        self.assertEqual(shot2png.rle_decode(bytes(encode(px))), px)

    def test_extrai_blocos_no_meio_do_log(self):
        log = "\n".join(["boot...", "[sync] 8 tarefas",
                         block("a", 2, 1, [0xF800, 0x001F]),
                         "ruido", block("b", 1, 1, [0xFFFF]), "fim"])
        shots = shot2png.parse_blocks(log)
        self.assertEqual([(s[0], s[1], s[2]) for s in shots], [("a", 2, 1), ("b", 1, 1)])
        self.assertEqual(shots[0][3], [0xF800, 0x001F])

    def test_bloco_com_tamanho_errado_e_recusado(self):
        with self.assertRaises(ValueError):
            shot2png.parse_blocks(block("x", 2, 2, [0, 0, 0]))

    def test_png_valido(self):
        data = shot2png.png_bytes(2, 1, [0xF800, 0x001F])
        self.assertEqual(data[:8], b"\x89PNG\r\n\x1a\n")
        w, h, depth, ctype = struct.unpack(">IIBB", data[16:26])
        self.assertEqual((w, h, depth, ctype), (2, 1, 8, 2))
        idat = data.index(b"IDAT")
        n = struct.unpack(">I", data[idat - 4:idat])[0]
        raw = zlib.decompress(data[idat + 4:idat + 4 + n])
        self.assertEqual(raw, bytes([0, 255, 0, 0, 0, 0, 255]))

    def test_main_grava_um_png_por_bloco(self):
        with tempfile.TemporaryDirectory() as d:
            log = os.path.join(d, "log.txt")
            with open(log, "w", encoding="utf-8") as f:
                f.write(block("foco", 1, 1, [0]) + "\n" + block("hoje", 1, 1, [0]))
            self.assertEqual(shot2png.main([log, "--out", d]), 0)
            self.assertTrue(os.path.exists(os.path.join(d, "foco.png")))
            self.assertTrue(os.path.exists(os.path.join(d, "hoje.png")))


if __name__ == "__main__":
    unittest.main()
