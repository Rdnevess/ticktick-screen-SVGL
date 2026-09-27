#!/usr/bin/env python3
"""
Pareamento de uso unico do TickTick Screen (spec 6.1).

Faz a danca do OAuth2 (authorization code) no seu PC e imprime o "blob de
pareamento" para colar no portal do aparelho. Nao guarda nada e nao fica
rodando: terminou, fechou.

Uso:
    python helper/pair.py                     # le helper/.env ou pergunta
    python helper/pair.py --send 192.168.0.42 # tambem envia direto ao aparelho

helper/.env (nunca versionado; modelo em helper/.env.example):
    CLIENT_ID=...
    CLIENT_SECRET=...
    REDIRECT_URI=http://127.0.0.1:8080/callback

A REDIRECT_URI tem de ser IDENTICA a cadastrada no app em
developer.ticktick.com (Manage Apps > seu app > OAuth redirect URL).

So biblioteca padrao.
"""
import argparse
import base64
import getpass
import http.server
import json
import os
import secrets
import socket
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
import webbrowser

AUTHORIZE_URL = "https://ticktick.com/oauth/authorize"
TOKEN_URL = "https://ticktick.com/oauth/token"
SCOPE = "tasks:read tasks:write"
DEFAULT_REDIRECT = "http://127.0.0.1:8080/callback"
DEFAULT_LIFETIME_S = 150 * 86400  # spec 6.4: conservador diante dos ~165-180 dias
DEVICE_HOST = "ticktick-screen.local"
HERE = os.path.dirname(os.path.abspath(__file__))


def load_env(path):
    """KEY=VALUE por linha; ignora comentarios, linhas vazias e aspas."""
    env = {}
    try:
        with open(path, encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith("#") or "=" not in line:
                    continue
                key, value = line.split("=", 1)
                env[key.strip()] = value.strip().strip('"').strip("'")
    except FileNotFoundError:
        pass
    return env


def build_authorize_url(client_id, redirect_uri, state):
    query = urllib.parse.urlencode({
        "client_id": client_id,
        "scope": SCOPE,
        "state": state,
        "redirect_uri": redirect_uri,
        "response_type": "code",
    })
    return AUTHORIZE_URL + "?" + query


def parse_callback(path, expected_state):
    """Extrai o code de /callback?code=...&state=... ; ValueError se algo nao bate."""
    qs = urllib.parse.parse_qs(urllib.parse.urlparse(path).query)
    if "error" in qs:
        raise ValueError("autorizacao negada: " + qs["error"][0])
    if qs.get("state", [""])[0] != expected_state:
        raise ValueError("state nao confere (possivel CSRF); rode de novo")
    code = qs.get("code", [""])[0]
    if not code:
        raise ValueError("retorno sem code")
    return code


def exchange_code(client_id, client_secret, code, redirect_uri,
                  urlopen=urllib.request.urlopen):
    body = urllib.parse.urlencode({
        "grant_type": "authorization_code",
        "code": code,
        "redirect_uri": redirect_uri,
        "scope": SCOPE,
    }).encode()
    basic = base64.b64encode(f"{client_id}:{client_secret}".encode()).decode()
    req = urllib.request.Request(TOKEN_URL, data=body, method="POST", headers={
        "Authorization": "Basic " + basic,
        "Content-Type": "application/x-www-form-urlencoded",
    })
    with urlopen(req, timeout=20) as resp:
        return json.loads(resp.read().decode())


def make_blob(client_id, client_secret, token, now=None):
    """base64 do JSON compacto que o portal do aparelho aceita (core/creds)."""
    now = int(time.time() if now is None else now)
    if "access_token" not in token:
        raise ValueError("resposta do token sem access_token: %r" % token)
    try:
        lifetime = int(token.get("expires_in") or 0)
    except (TypeError, ValueError):
        lifetime = 0
    if lifetime <= 0:  # ausente, 0 ou lixo: prazo conservador (spec 6.4)
        lifetime = DEFAULT_LIFETIME_S
    payload = {
        "cid": client_id,
        "csec": client_secret,
        "atok": token["access_token"],
        "rtok": token.get("refresh_token", ""),
        "exp": now + lifetime,
    }
    raw = json.dumps(payload, separators=(",", ":")).encode()
    return base64.b64encode(raw).decode()


def wait_for_code(redirect_uri, state, timeout_s=300):
    """Sobe o servidor do redirect, espera UM retorno do navegador e fecha."""
    u = urllib.parse.urlparse(redirect_uri)
    host, port, cb_path = u.hostname, u.port or 80, u.path
    result = {}

    class Handler(http.server.BaseHTTPRequestHandler):
        def do_GET(self):
            if urllib.parse.urlparse(self.path).path != cb_path:
                self.send_response(404)
                self.end_headers()
                return
            try:
                result["code"] = parse_callback(self.path, state)
                msg = "Pronto! Pode fechar esta aba e voltar ao terminal."
            except ValueError as e:
                result["error"] = str(e)
                msg = "Falhou: " + str(e)
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.end_headers()
            self.wfile.write((
                "<html><body style='font-family:sans-serif;background:#0E1116;"
                "color:#E6EDF3;padding:40px'><h2>TickTick Screen</h2>"
                f"<p>{msg}</p></body></html>").encode())

        def log_message(self, *args):
            pass  # sem barulho no terminal

    srv = http.server.HTTPServer((host, port), Handler)
    srv.timeout = 1
    deadline = time.time() + timeout_s
    while not result and time.time() < deadline:
        srv.handle_request()
    srv.server_close()
    if "error" in result:
        raise ValueError(result["error"])
    if "code" not in result:
        raise TimeoutError("nenhum retorno do navegador em %ds" % timeout_s)
    return result["code"]


def find_device(host=DEVICE_HOST):
    """mDNS pelo resolvedor do sistema. No Windows falha com frequencia — por
    isso colar o blob e o caminho principal e isto e so um atalho."""
    try:
        return socket.gethostbyname(host)
    except OSError:
        return None


def send_to_device(ip, blob, urlopen=urllib.request.urlopen):
    body = urllib.parse.urlencode({"blob": blob}).encode()
    req = urllib.request.Request(f"http://{ip}/pair", data=body, method="POST")
    with urlopen(req, timeout=10) as resp:
        return resp.status


def friendly_error(e):
    """Uma linha para o terminal no lugar de um traceback."""
    if isinstance(e, urllib.error.HTTPError):
        try:
            detail = e.read().decode(errors="replace")[:200]
        except Exception:  # corpo ja lido ou ausente
            detail = ""
        return f"o TickTick respondeu HTTP {e.code} {detail}".strip()
    if isinstance(e, (TimeoutError, socket.timeout)):
        return f"tempo esgotado: {e}"
    if isinstance(e, urllib.error.URLError):
        return f"sem conexão com o TickTick ({e.reason}); confira a internet e tente de novo"
    if isinstance(e, ValueError):
        return str(e)
    return f"{type(e).__name__}: {e}"


def main(argv=None):
    ap = argparse.ArgumentParser(description="Pareamento do TickTick Screen")
    ap.add_argument("--send", metavar="IP", help="envia o blob direto ao aparelho neste IP")
    ap.add_argument("--env", default=os.path.join(HERE, ".env"),
                    help="arquivo com CLIENT_ID, CLIENT_SECRET e REDIRECT_URI")
    args = ap.parse_args(argv)

    env = load_env(args.env)
    cid = env.get("CLIENT_ID") or input("Client ID: ").strip()
    csec = env.get("CLIENT_SECRET") or getpass.getpass("Client Secret: ").strip()
    redirect = env.get("REDIRECT_URI") or DEFAULT_REDIRECT
    state = secrets.token_urlsafe(16)

    url = build_authorize_url(cid, redirect, state)
    print("Abrindo o navegador para autorizar. Se nao abrir, visite:\n  " + url)
    webbrowser.open(url)
    try:
        code = wait_for_code(redirect, state)
        token = exchange_code(cid, csec, code, redirect)
        blob = make_blob(cid, csec, token)
    except (OSError, ValueError) as e:  # URLError, HTTPError e TimeoutError sao OSError
        sys.exit("pareamento falhou: " + friendly_error(e))

    print("\nCole este blob no portal do aparelho (campo unico):\n")
    print(blob)
    print()

    ip = args.send or find_device()
    if ip:
        ask = f"Aparelho encontrado em {ip}. Enviar direto? [s/N] "
        if args.send or input(ask).strip().lower() == "s":
            try:
                status = send_to_device(ip, blob)
                print(f"enviado: HTTP {status} — confira a tela do aparelho")
            except OSError as e:
                print(f"nao deu para enviar ({e}); cole o blob no portal")
    return 0


if __name__ == "__main__":
    sys.exit(main())
