#!/usr/bin/env python3
"""
Captura respostas reais da Open API do TickTick e grava versoes ANONIMIZADAS
em tests/fixtures/. Transforma em evidencia tres premissas da spec: o formato
das tarefas de dia inteiro (5.2), a Entrada via /project/inbox/data e os
campos que o filtro do parser deixa passar.

Uso:
    python tools/capture_payload.py              # primeiras 3 listas + Entrada
    python tools/capture_payload.py --lists ID1,ID2
    python tools/capture_payload.py --blob-file C:/caminho/blob.txt

Pede o blob do helper sem ecoar (ou le de --blob-file, sem nunca imprimir o
conteudo). Titulos, nomes, descricoes, tags e ids sao trocados; datas, fusos,
prioridades, status e sortOrder ficam como vieram, porque sao exatamente o
que se quer observar.
"""
import argparse
import base64
import getpass
import hashlib
import json
import os
import sys
import time
import urllib.error
import urllib.request

API = "https://api.ticktick.com/open/v1"
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "tests", "fixtures")
TEXT_FIELDS = {"title", "name", "content", "desc", "description", "tags", "columnName"}
ID_FIELDS = {"id", "projectId", "parentId", "columnId", "groupId", "childIds"}


def get(path, token):
    req = urllib.request.Request(API + path, headers={"Authorization": "Bearer " + token})
    try:
        with urllib.request.urlopen(req, timeout=20) as resp:
            return resp.status, json.loads(resp.read().decode())
    except urllib.error.HTTPError as e:
        return e.code, None


class Anon:
    """Troca texto e ids de forma deterministica: o mesmo id real vira sempre o
    mesmo id falso, entao tarefa e lista continuam casando."""

    def __init__(self):
        self.n = 0

    @staticmethod
    def fake_id(real):
        digest = hashlib.sha1(real.encode()).hexdigest()
        if real.startswith("inbox"):  # o id da Entrada tem forma propria
            return "inbox" + digest[:10]
        return digest[:24]

    def scrub(self, obj, key=None):
        if isinstance(obj, dict):
            return {k: self.scrub(v, k) for k, v in obj.items()}
        if isinstance(obj, list):
            return [self.scrub(v, key) for v in obj]
        if isinstance(obj, str) and obj:
            if key in TEXT_FIELDS:
                self.n += 1
                return f"Texto {self.n:03d}"
            if key in ID_FIELDS:
                return self.fake_id(obj)
        return obj


def save(name, data):
    path = os.path.join(OUT, name)
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        json.dump(data, f, ensure_ascii=False, indent=1)
        f.write("\n")
    print("  gravado", os.path.relpath(path, ROOT))


def report(label, data):
    """Imprime so o que interessa e nao identifica ninguem."""
    tasks = (data or {}).get("tasks", [])
    keys = sorted({k for t in tasks for k in t})
    print(f"  {label}: {len(tasks)} tarefas; campos: {', '.join(keys)}")
    for t in tasks:
        if t.get("isAllDay"):
            print(f"    dia inteiro: dueDate={t.get('dueDate')!r} "
                  f"startDate={t.get('startDate')!r} timeZone={t.get('timeZone')!r}")
    timed = [t for t in tasks if t.get("dueDate") and not t.get("isAllDay")]
    if timed:
        t = timed[0]
        print(f"    com hora (exemplo): dueDate={t.get('dueDate')!r} timeZone={t.get('timeZone')!r}")
    for field in ("priority", "sortOrder", "status"):
        types = sorted({type(t[field]).__name__ for t in tasks if field in t})
        if types:
            print(f"    tipo de {field}: {', '.join(types)}")


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[1])
    ap.add_argument("--lists", help="ids separados por virgula (padrao: as 3 primeiras)")
    ap.add_argument("--max", type=int, default=3)
    ap.add_argument("--blob-file", metavar="PATH",
                     help="le o blob deste arquivo (strip) em vez de pedir por prompt; "
                          "conteudo nunca e impresso, copiado ou logado")
    args = ap.parse_args()

    if args.blob_file:
        with open(args.blob_file, "r", encoding="utf-8") as f:
            blob = f.read().strip()
    else:
        blob = getpass.getpass("Blob do helper (nao aparece ao colar): ").strip()
    token = json.loads(base64.b64decode(blob))["atok"]
    del blob
    os.makedirs(OUT, exist_ok=True)
    anon = Anon()

    code, projects = get("/project", token)
    print(f"GET /project -> HTTP {code}")
    if code != 200:
        sys.exit("sem a lista de projetos nao ha o que capturar")
    save("projects.json", anon.scrub(projects))

    ids = args.lists.split(",") if args.lists else [p["id"] for p in projects][: args.max]
    for i, pid in enumerate(ids, 1):
        code, data = get(f"/project/{pid}/data", token)
        print(f"GET /project/<lista {i}>/data -> HTTP {code}")
        if code == 200:
            report(f"lista {i}", data)
            save(f"project_data_{i}.json", anon.scrub(data))

    inbox_code, inbox = get("/project/inbox/data", token)
    print(f"GET /project/inbox/data -> HTTP {inbox_code}")
    if inbox_code == 200:
        report("Entrada", inbox)
        save("inbox_data.json", anon.scrub(inbox))

    # Linha 1 e lida pelo teste de host: "agora" local e fuso no momento da
    # captura, para o filtro do dia ver o mesmo dia que voce via no app.
    now = int(time.time())
    tz = time.localtime(now).tm_gmtoff
    with open(os.path.join(OUT, "CAPTURA.txt"), "w", encoding="utf-8", newline="\n") as f:
        f.write(f"{now + tz} {tz}\n")
        f.write(f"capturado em {time.strftime('%Y-%m-%d %H:%M %z', time.localtime(now))}\n")
        f.write(f"inbox: HTTP {inbox_code}\n")
    print("  gravado tests/fixtures/CAPTURA.txt")


if __name__ == "__main__":
    main()
