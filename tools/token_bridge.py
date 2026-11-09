#!/usr/bin/env python3
"""
token_bridge.py — envia a contagem de tokens das sessoes locais do Claude Code
para o Claude Usage Stick.

Por que existe: a API da Anthropic NAO expoe contagem de tokens para contas de
assinatura (os headers unified-* so trazem utilizacao em %). Quem tem os numeros
reais sao os transcripts locais do Claude Code (~/.claude/projects/**/*.jsonl),
que registram `usage` por mensagem (inclusive os de subagentes, em subpastas).
Este script soma os tokens da janela de 5h atual (inicio = reset - 5h, obtido do
proprio device via GET /window?account=<rotulo>) e faz POST /tokens no gadget.

Varias contas no mesmo computador (firmware v3): cada conta roda o Claude Code
com o proprio CLAUDE_CONFIG_DIR e tem o proprio bridge:
    alias claude-a='CLAUDE_CONFIG_DIR=~/.claude-a claude'
    python3 tools/token_bridge.py --config-dir ~/.claude-a --account A --loop 120

Uso:
    python3 tools/token_bridge.py                 # um envio
    python3 tools/token_bridge.py --loop 120      # reenvia a cada 120s
    python3 tools/token_bridge.py --host 192.168.1.50
    python3 tools/token_bridge.py --account Trabalho --loop 120
    python3 tools/token_bridge.py --config-dir ~/.claude-ti --account TI --loop 120

Agendar no macOS (a cada 2 min, exemplo com cron):
    */2 * * * * /usr/bin/python3 /caminho/token_bridge.py >/dev/null 2>&1

Sem dependencias externas (stdlib).
"""
import argparse
import glob
import json
import os
import time
import urllib.error
import urllib.parse
import urllib.request
from datetime import datetime

DEFAULT_CONFIG_DIR = "~/.claude"


def parse_ts(ts: str) -> float:
    """ISO-8601 (com Z) -> epoch. Retorna 0 se nao parsear."""
    try:
        return datetime.fromisoformat(ts.replace("Z", "+00:00")).timestamp()
    except Exception:
        return 0.0


def session_key(path: str, projects_dir: str) -> str:
    """Sessao a que o transcript pertence: <projeto>/<sessao>.jsonl e os
    subagentes em <projeto>/<sessao>/subagents/*.jsonl contam como uma so."""
    parts = os.path.relpath(path, projects_dir).split(os.sep)
    if len(parts) >= 3:
        return os.path.join(parts[0], parts[1])
    return os.path.splitext(os.path.join(*parts))[0]


def collect(start: float, projects_dir: str):
    """Soma usage de todas as mensagens com timestamp >= start (dedupe por id).

    Percorre projects_dir recursivamente: os transcripts de subagentes ficam em
    subpastas (<projeto>/<sessao>/subagents/*.jsonl).
    """
    tin = tout = tcache = 0
    seen = set()
    sessions = set()
    pattern = os.path.join(glob.escape(projects_dir), "**", "*.jsonl")
    for path in sorted(glob.glob(pattern, recursive=True)):
        try:
            if os.path.getmtime(path) < start - 60:
                continue  # arquivo nao mexido desde antes da janela
            with open(path, "r", encoding="utf-8", errors="ignore") as fh:
                for line in fh:
                    if '"usage"' not in line:
                        continue
                    try:
                        rec = json.loads(line)
                    except Exception:
                        continue
                    msg = rec.get("message") or {}
                    usage = msg.get("usage")
                    if not usage:
                        continue
                    if parse_ts(rec.get("timestamp", "")) < start:
                        continue
                    key = msg.get("id") or rec.get("uuid")
                    if key and key in seen:
                        continue  # mensagens repetidas no transcript
                    if key:
                        seen.add(key)
                    tin += usage.get("input_tokens", 0) or 0
                    tin += usage.get("cache_creation_input_tokens", 0) or 0
                    tout += usage.get("output_tokens", 0) or 0
                    tcache += usage.get("cache_read_input_tokens", 0) or 0
                    sessions.add(session_key(path, projects_dir))
        except OSError:
            continue
    return tin, tout, tcache, len(sessions)


NOT_FOUND_MSG = "rotulo nao existe no gadget"


def push(host: str, account=None, projects_dir=None) -> bool:
    """Um envio. Devolve True se o gadget aceitou a contagem."""
    if projects_dir is None:
        projects_dir = os.path.join(os.path.expanduser(DEFAULT_CONFIG_DIR), "projects")
    who = f" [{account}]" if account else ""

    # janela atual direto do device (h5_reset vem dos headers unified-*)
    url = f"http://{host}/window"
    if account:
        url += "?account=" + urllib.parse.quote(account, safe="")
    try:
        with urllib.request.urlopen(url, timeout=6) as r:
            win = json.load(r)
    except urllib.error.HTTPError as e:
        e.close()
        if e.code == 404:
            print(f"[bridge]{who} {NOT_FOUND_MSG} — push ignorado")
            return False
        raise

    # Aparelho v2.3 ignora ?account= e devolve a conta ativa: nao envia a
    # contagem para a conta errada (no v3 o rotulo devolvido e o pedido).
    active = win.get("account")
    if account and active and account != active:
        print(f"[bridge] device monitorando '{active}', nao '{account}' — aguardando")
        return False

    reset = int(win.get("h5_reset") or 0)
    now = time.time()
    start = (reset - 5 * 3600) if reset > now - 86400 else now - 5 * 3600

    tin, tout, tcache, nsess = collect(start, projects_dir)
    body = {"in": tin, "out": tout, "cache": tcache, "sessions": nsess}
    if account:
        body["account"] = account
    # UTF-8 cru (ensure_ascii=False): o firmware compara o rotulo byte a byte
    # e nao decodifica \uXXXX.
    req = urllib.request.Request(
        f"http://{host}/tokens", data=json.dumps(body, ensure_ascii=False).encode("utf-8"),
        headers={"Content-Type": "application/json; charset=utf-8"}, method="POST",
    )
    try:
        with urllib.request.urlopen(req, timeout=6) as r:
            r.read()
    except urllib.error.HTTPError as e:
        e.close()
        if e.code == 404:
            print(f"[bridge]{who} {NOT_FOUND_MSG} — push ignorado")
            return False
        if e.code == 409:   # so aparelhos v2.3 (account_mismatch)
            print("[bridge] device trocou de conta ativa — push ignorado")
            return False
        raise
    t0 = datetime.fromtimestamp(start).strftime("%H:%M")
    print(f"[bridge]{who} janela desde {t0}: entrada={tin:,} saida={tout:,} "
          f"cache={tcache:,} sessoes={nsess} -> {host} ok")
    return True


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--host", default="caio-thep-usage-monitor.local",
                    help="hostname/IP do gadget (default: caio-thep-usage-monitor.local)")
    ap.add_argument("--loop", type=int, default=0, metavar="SEG",
                    help="repetir a cada SEG segundos (0 = um envio)")
    ap.add_argument("--account", default=None, metavar="ROTULO",
                    help="rotulo da conta deste host no gadget (multi-conta); "
                         "sem ele o push vale para a conta em foco, seja qual for")
    ap.add_argument("--config-dir", default=DEFAULT_CONFIG_DIR, metavar="DIR",
                    help="CLAUDE_CONFIG_DIR da conta; le DIR/projects "
                         "(default: ~/.claude)")
    args = ap.parse_args()
    projects_dir = os.path.join(os.path.expanduser(args.config_dir), "projects")
    if not os.path.isdir(projects_dir):
        print(f"[bridge] aviso: {projects_dir} nao existe (contagem fica em 0)")
    while True:
        try:
            push(args.host, args.account, projects_dir)
        except Exception as e:
            print(f"[bridge] erro: {e}")
        if not args.loop:
            break
        time.sleep(args.loop)


if __name__ == "__main__":
    main()
