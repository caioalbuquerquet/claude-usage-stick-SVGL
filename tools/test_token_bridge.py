#!/usr/bin/env python3
"""
Testes do tools/token_bridge.py (só stdlib, sem rede até o gadget).

    python3 -I tools/test_token_bridge.py

collect(): árvore temporária com transcripts em subpastas (subagentes),
dedupe por message.id, corte pela janela e pelo mtime.
push(): servidor http.server local que imita o gadget (200, 404 e o 409 da v2.3).
"""
import contextlib
import importlib.util
import io
import json
import os
import tempfile
import threading
import time
import unittest
import urllib.parse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

# -I não põe a pasta do script no sys.path: carrega pelo caminho.
_HERE = os.path.dirname(os.path.abspath(__file__))
_spec = importlib.util.spec_from_file_location("token_bridge", os.path.join(_HERE, "token_bridge.py"))
tb = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(tb)


def iso(epoch: float) -> str:
    return time.strftime("%Y-%m-%dT%H:%M:%S.000Z", time.gmtime(epoch))


def rec(epoch, mid, inp=0, out=0, cc=0, cr=0, uuid=None):
    r = {"timestamp": iso(epoch), "uuid": uuid or f"u-{mid}",
         "message": {"id": mid, "usage": {"input_tokens": inp, "output_tokens": out,
                                          "cache_creation_input_tokens": cc,
                                          "cache_read_input_tokens": cr}}}
    return json.dumps(r)


def write(path, lines):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as fh:
        fh.write("\n".join(lines) + "\n")


class CollectTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.projects = os.path.join(self.tmp.name, "cfg", "projects")
        self.now = time.time()
        self.start = self.now - 3600
        p = self.projects
        # sessão principal do projeto A
        write(os.path.join(p, "proj-a", "s1.jsonl"), [
            rec(self.now - 600, "m1", inp=10, out=20, cc=5, cr=100),
            rec(self.now - 600, "m1", inp=10, out=20, cc=5, cr=100),   # repetida
            rec(self.now - 7200, "m0", inp=999, out=999),             # antes da janela
            '{"type":"user","message":{"content":"sem usage"}}',
            "linha quebrada {\"usage\"",
        ])
        # subagentes da mesma sessão, em subpasta (o glob antigo não via)
        write(os.path.join(p, "proj-a", "s1", "subagents", "agent-1.jsonl"), [
            rec(self.now - 300, "m2", inp=1, out=2, cr=3),
            rec(self.now - 300, "m1", inp=10, out=20, cc=5, cr=100),   # mesmo id da principal
        ])
        # outro projeto, outra sessão
        write(os.path.join(p, "proj-b", "s2.jsonl"), [
            rec(self.now - 60, "m3", inp=100, out=200, cr=1000),
        ])
        # arquivo antigo (mtime antes da janela) é pulado sem ler
        old = os.path.join(p, "proj-c", "velho.jsonl")
        write(old, [rec(self.now - 60, "m9", inp=5000)])
        os.utime(old, (self.now - 86400, self.now - 86400))
        # outro CLAUDE_CONFIG_DIR, que não deve entrar
        write(os.path.join(self.tmp.name, "outra", "projects", "proj-x", "s.jsonl"),
              [rec(self.now - 60, "mx", inp=77777)])

    def tearDown(self):
        self.tmp.cleanup()

    def test_soma_recursiva_com_dedupe(self):
        tin, tout, tcache, nsess = tb.collect(self.start, self.projects)
        self.assertEqual(tin, 10 + 5 + 1 + 100)    # input + cache_creation
        self.assertEqual(tout, 20 + 2 + 200)
        self.assertEqual(tcache, 100 + 3 + 1000)
        self.assertEqual(nsess, 2)                  # s1 (com subagente) e s2

    def test_dir_inexistente(self):
        self.assertEqual(tb.collect(self.start, os.path.join(self.tmp.name, "nada")), (0, 0, 0, 0))

    def test_session_key(self):
        p = self.projects
        self.assertEqual(tb.session_key(os.path.join(p, "a", "s1.jsonl"), p), os.path.join("a", "s1"))
        self.assertEqual(tb.session_key(os.path.join(p, "a", "s1", "subagents", "x.jsonl"), p),
                         os.path.join("a", "s1"))


class FakeStick(BaseHTTPRequestHandler):
    """Imita o gadget. server.labels: rótulos existentes; server.focused: foco;
    server.v23: comporta-se como a v2.3 (ignora ?account=, 409 no mismatch)."""

    def log_message(self, *a):
        pass

    def _json(self, code, obj):
        b = json.dumps(obj, ensure_ascii=False).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(b)))
        self.end_headers()
        self.wfile.write(b)

    def do_GET(self):
        srv = self.server
        u = urllib.parse.urlsplit(self.path)
        srv.requests.append(("GET", self.path, None))
        if u.path != "/window":
            return self._json(404, {"error": "not_found"})
        q = urllib.parse.parse_qs(u.query)
        lbl = q.get("account", [""])[0]
        if srv.v23 or not lbl:
            lbl = srv.focused
        if lbl not in srv.labels:
            return self._json(404, {"error": "unknown_account", "account": lbl})
        self._json(200, {"now": int(time.time()), "h5_reset": int(time.time()) + 3600,
                         "d7_reset": 0, "h5_util": 0.1, "d7_util": 0.2,
                         "account": lbl, "slot": srv.labels.index(lbl)})

    def do_POST(self):
        srv = self.server
        n = int(self.headers.get("Content-Length") or 0)
        raw = self.rfile.read(n)
        srv.raw_posts.append(raw)
        body = json.loads(raw.decode("utf-8"))
        srv.requests.append(("POST", self.path, body))
        lbl = body.get("account") or srv.focused
        if srv.v23 and lbl != srv.focused:
            return self._json(409, {"error": "account_mismatch", "active": srv.focused})
        if lbl not in srv.labels or srv.post_404:
            return self._json(404, {"error": "unknown_account", "account": lbl})
        self._json(200, {"ok": True})


class PushTest(unittest.TestCase):
    def setUp(self):
        self.srv = ThreadingHTTPServer(("127.0.0.1", 0), FakeStick)
        self.srv.labels = ["Caio", "TI", "Ação"]
        self.srv.focused = "Caio"
        self.srv.v23 = False
        self.srv.post_404 = False
        self.srv.requests = []
        self.srv.raw_posts = []
        threading.Thread(target=self.srv.serve_forever, daemon=True).start()
        self.host = f"127.0.0.1:{self.srv.server_address[1]}"
        self.tmp = tempfile.TemporaryDirectory()
        self.projects = os.path.join(self.tmp.name, "projects")
        write(os.path.join(self.projects, "p", "s.jsonl"), [rec(time.time() - 60, "m1", inp=7, out=8, cr=9)])

    def tearDown(self):
        self.srv.shutdown()
        self.srv.server_close()
        self.tmp.cleanup()

    def push(self, account):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            ok = tb.push(self.host, account, self.projects)
        return ok, out.getvalue()

    def test_200_conta_fora_do_foco(self):
        ok, log = self.push("TI")
        self.assertTrue(ok, log)
        get, post = self.srv.requests
        self.assertEqual(get[1], "/window?account=TI")
        self.assertEqual(post[1], "/tokens")
        self.assertEqual(post[2], {"in": 7, "out": 8, "cache": 9, "sessions": 1, "account": "TI"})
        self.assertIn("[TI]", log)

    def test_rotulo_codificado(self):
        ok, log = self.push("Ação")
        self.assertTrue(ok, log)
        self.assertEqual(self.srv.requests[0][1], "/window?account=A%C3%A7%C3%A3o")
        self.assertEqual(self.srv.requests[1][2]["account"], "Ação")
        # o firmware compara os bytes UTF-8 e não decodifica \uXXXX (jstr)
        self.assertIn('"account": "Ação"'.encode("utf-8"), self.srv.raw_posts[0])
        self.assertNotIn(b"\\u", self.srv.raw_posts[0])

    def test_sem_conta_vai_para_o_foco(self):
        ok, _ = self.push(None)
        self.assertTrue(ok)
        self.assertEqual(self.srv.requests[0][1], "/window")
        self.assertNotIn("account", self.srv.requests[1][2])

    def test_404_no_window(self):
        ok, log = self.push("Pessoal")
        self.assertFalse(ok)
        self.assertIn(tb.NOT_FOUND_MSG, log)
        self.assertEqual(len(self.srv.requests), 1)   # nem tenta o POST

    def test_404_no_post(self):
        self.srv.post_404 = True                      # conta removida entre GET e POST
        ok, log = self.push("TI")
        self.assertFalse(ok)
        self.assertIn(tb.NOT_FOUND_MSG, log)

    def test_v23_conta_diferente_aguarda(self):
        self.srv.v23 = True                           # ignora ?account= e devolve o foco
        ok, log = self.push("TI")
        self.assertFalse(ok)
        self.assertIn("aguardando", log)
        self.assertEqual(len(self.srv.requests), 1)

    def test_v23_409(self):
        self.srv.v23 = True
        self.srv.focused = "Caio"
        # GET devolve Caio (bate), mas o foco troca antes do POST
        orig = FakeStick.do_POST

        def post_after_switch(h):
            h.server.focused = "TI"
            orig(h)
        FakeStick.do_POST = post_after_switch
        try:
            ok, log = self.push("Caio")
        finally:
            FakeStick.do_POST = orig
        self.assertFalse(ok)
        self.assertIn("trocou de conta", log)

    def test_outro_erro_sobe(self):
        with self.assertRaises(Exception):
            tb.push("127.0.0.1:1", "TI", self.projects)   # porta fechada


if __name__ == "__main__":
    unittest.main(verbosity=2)
