"""Testes do helper de pareamento. Rodar: python -m unittest discover -s tests/helper -v"""
import base64
import io
import json
import os
import socket
import sys
import tempfile
import unittest
import urllib.error
import urllib.parse

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "helper"))
import pair  # noqa: E402


class TestAutorizacao(unittest.TestCase):
    def test_url_de_autorizacao_tem_todos_os_parametros(self):
        url = pair.build_authorize_url("cid1", "http://127.0.0.1:8080/callback", "st")
        self.assertTrue(url.startswith("https://ticktick.com/oauth/authorize?"))
        q = urllib.parse.parse_qs(urllib.parse.urlparse(url).query)
        self.assertEqual(q["client_id"], ["cid1"])
        self.assertEqual(q["scope"], ["tasks:read tasks:write"])
        self.assertEqual(q["response_type"], ["code"])
        self.assertEqual(q["state"], ["st"])
        self.assertEqual(q["redirect_uri"], ["http://127.0.0.1:8080/callback"])

    def test_callback_devolve_o_code(self):
        self.assertEqual(pair.parse_callback("/callback?code=abc&state=st", "st"), "abc")

    def test_callback_com_state_errado_e_recusado(self):
        with self.assertRaises(ValueError):
            pair.parse_callback("/callback?code=abc&state=outro", "st")

    def test_callback_com_erro_e_recusado(self):
        with self.assertRaises(ValueError):
            pair.parse_callback("/callback?error=access_denied&state=st", "st")

    def test_callback_sem_code_e_recusado(self):
        with self.assertRaises(ValueError):
            pair.parse_callback("/callback?state=st", "st")


class TestToken(unittest.TestCase):
    def test_troca_do_code_manda_basic_auth_e_formulario(self):
        seen = {}

        def fake_urlopen(req, timeout):
            seen["req"] = req
            return io.BytesIO(b'{"access_token":"a"}')

        tok = pair.exchange_code("cid", "sec", "code1", "http://x/cb", urlopen=fake_urlopen)
        req = seen["req"]
        self.assertEqual(tok["access_token"], "a")
        self.assertEqual(req.full_url, "https://ticktick.com/oauth/token")
        self.assertEqual(req.get_header("Authorization"),
                         "Basic " + base64.b64encode(b"cid:sec").decode())
        form = urllib.parse.parse_qs(req.data.decode())
        self.assertEqual(form["grant_type"], ["authorization_code"])
        self.assertEqual(form["code"], ["code1"])
        self.assertEqual(form["redirect_uri"], ["http://x/cb"])


class TestBlob(unittest.TestCase):
    def test_blob_e_base64_de_json_compacto(self):
        blob = pair.make_blob("c", "s", {"access_token": "a", "refresh_token": "r",
                                         "expires_in": 100}, now=1000)
        self.assertNotIn(" ", base64.b64decode(blob).decode())
        self.assertEqual(json.loads(base64.b64decode(blob)),
                         {"cid": "c", "csec": "s", "atok": "a", "rtok": "r", "exp": 1100})

    def test_sem_expires_in_usa_150_dias_e_rtok_vazio(self):
        d = json.loads(base64.b64decode(pair.make_blob("c", "s", {"access_token": "a"}, now=0)))
        self.assertEqual(d["exp"], 150 * 86400)
        self.assertEqual(d["rtok"], "")

    def test_sem_access_token_e_erro(self):
        with self.assertRaises(ValueError):
            pair.make_blob("c", "s", {"error": "invalid_grant"}, now=0)

    def test_expires_in_zero_ou_negativo_conta_como_ausente(self):
        for v in (0, "0", -5, None):
            d = json.loads(base64.b64decode(
                pair.make_blob("c", "s", {"access_token": "a", "expires_in": v}, now=0)))
            self.assertEqual(d["exp"], 150 * 86400, v)


class TestEnv(unittest.TestCase):
    def test_env_ignora_comentarios_e_aspas(self):
        with tempfile.NamedTemporaryFile("w", suffix=".env", delete=False,
                                         encoding="utf-8") as f:
            f.write("# comentario\nCLIENT_ID=abc\nCLIENT_SECRET=\"xyz\"\n\nlixo sem igual\n")
            path = f.name
        try:
            self.assertEqual(pair.load_env(path), {"CLIENT_ID": "abc", "CLIENT_SECRET": "xyz"})
        finally:
            os.unlink(path)

    def test_env_inexistente_devolve_vazio(self):
        self.assertEqual(pair.load_env("nao/existe/.env"), {})


class TestErros(unittest.TestCase):
    def test_http_error_mostra_o_codigo(self):
        e = urllib.error.HTTPError("u", 401, "Unauthorized", {}, io.BytesIO(b'{"error":"x"}'))
        msg = pair.friendly_error(e)
        self.assertIn("HTTP 401", msg)
        self.assertNotIn("Traceback", msg)

    def test_sem_rede(self):
        msg = pair.friendly_error(urllib.error.URLError("getaddrinfo failed"))
        self.assertIn("sem conexão", msg)

    def test_tempo_esgotado(self):
        self.assertIn("tempo esgotado", pair.friendly_error(TimeoutError("x")))
        self.assertIn("tempo esgotado", pair.friendly_error(socket.timeout("x")))

    def test_autorizacao_negada_passa_a_mensagem(self):
        msg = pair.friendly_error(ValueError("autorizacao negada: access_denied"))
        self.assertIn("access_denied", msg)


if __name__ == "__main__":
    unittest.main()
