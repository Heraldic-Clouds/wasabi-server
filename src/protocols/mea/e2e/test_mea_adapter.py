#!/usr/bin/env python3
# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements; see the NOTICE file. Apache License 2.0.
#
# End-to-end test for libguac-client-mea: a real guacd with the plugin, the
# real MEA Stream agent capturing Xvfb, and a minimal Guacamole protocol
# client. Checks that the desktop is drawn, mouse and keyboard reach X, and
# that refused tokens end the connection as CLIENT_UNAUTHORIZED.
#
#   BIN=<dir with mea-stream-agent, mea-stream-token, xprobe>  (Type3 stream/)
#   GUACD_BUILD=<built wasabi-server tree>
#   python3 -I test_mea_adapter.py
import json, os, socket, subprocess, sys, tempfile, time

BIN = os.environ["BIN"]
BUILD = os.environ.get("GUACD_BUILD", ".")
DISPLAY = os.environ.get("E2E_DISPLAY", ":79")
GUACD_PORT, AGENT_PORT, DESKTOP = 4829, 47109, "desk-guac"
procs, failures = [], 0


def check(ok, what, extra=""):
    global failures
    print(("PASS " if ok else "FAIL ") + what + (" - " + extra if extra else ""))
    failures += 0 if ok else 1


def run(*cmd, env=None, log=None):
    p = subprocess.Popen(cmd, env={**os.environ, **(env or {})},
                         stdout=log or subprocess.DEVNULL, stderr=log or subprocess.DEVNULL)
    procs.append(p)
    return p


def probe():
    out = subprocess.check_output([os.path.join(BIN, "xprobe")], env={**os.environ, "DISPLAY": DISPLAY},
                                  stderr=subprocess.DEVNULL)
    return json.loads(out)


def wait(cond, secs):
    end = time.time() + secs
    while time.time() < end:
        try:
            v = cond()
            if v:
                return v
        except Exception:
            pass
        time.sleep(0.1)
    return None


def enc(*args):
    return ",".join(f"{len(str(a))}.{a}" for a in args) + ";"


class Guac:
    def __init__(self):
        self.s = socket.create_connection(("127.0.0.1", GUACD_PORT), timeout=10)
        self.buf = b""

    def send(self, *args):
        self.s.sendall(enc(*args).encode())

    def read(self):
        while b";" not in self.buf or not self._complete():
            d = self.s.recv(65536)
            if not d:
                raise EOFError
            self.buf += d
        return self._pop()

    def _complete(self):
        try:
            self._parse(peek=True)
            return True
        except IndexError:
            return False

    def _parse(self, peek=False):
        s, i, out = self.buf.decode("utf-8", "replace"), 0, []
        while True:
            dot = s.index(".", i)
            n = int(s[i:dot])
            val = s[dot + 1:dot + 1 + n]
            if dot + 1 + n >= len(s):
                raise IndexError
            out.append(val)
            term = s[dot + 1 + n]
            i = dot + 2 + n
            if term == ";":
                if not peek:
                    self.buf = s[i:].encode()
                return out

    def _pop(self):
        return self._parse()


def mint():
    return subprocess.check_output([os.path.join(BIN, "mea-stream-token"), "mint", "-desktop", DESKTOP,
                                    "-host", "any"], env={**os.environ, "MEA_STREAM_PRIVATE_KEY": keys["MEA_STREAM_PRIVATE_KEY"]}).decode().strip()


def connect(token):
    g = Guac()
    g.send("select", "mea")
    args = g.read()
    assert args[0] == "args", args
    values = {"hostname": "127.0.0.1", "port": str(AGENT_PORT), "token": token, "profile": "economy",
              "fps": "30", "kbps": "3000", "read-only": ""}
    g.send("size", 1280, 720, 96)
    g.send("audio")
    g.send("video")
    g.send("image", "image/png", "image/jpeg")
    g.send("connect", args[1], *[values[a] for a in args[2:]])
    return g


def pump(g, secs, until=None):
    """Reads instructions, answering sync, for up to secs. Returns counts."""
    seen, end = {}, time.time() + secs
    g.s.settimeout(0.5)
    while time.time() < end:
        try:
            ins = g.read()
        except socket.timeout:
            continue
        seen[ins[0]] = seen.get(ins[0], 0) + 1
        if ins[0] == "sync":
            g.send("sync", ins[1])
        if ins[0] in ("error", "disconnect"):
            seen.setdefault("_error", ins)
            break
        if until and until(ins, seen):
            break
    return seen


tmp = tempfile.mkdtemp()
keys = dict(l.split("=", 1) for l in subprocess.check_output([os.path.join(BIN, "mea-stream-token"), "keygen"]).decode().split())
try:
    run("Xvfb", DISPLAY, "-screen", "0", "1280x720x24", "-nolisten", "tcp")
    wait(probe, 5)
    run(os.path.join(BIN, "xprobe"), "paint", env={"DISPLAY": DISPLAY})
    agent_log = open(os.path.join(tmp, "agent.log"), "w")
    run(os.path.join(BIN, "mea-stream-agent"), log=agent_log, env={
        "DISPLAY": DISPLAY, "MEA_STREAM_PUBLIC_KEY": keys["MEA_STREAM_PUBLIC_KEY"], "MEA_DESKTOP_ID": DESKTOP,
        "MEA_ENCODER": "x264", "MEA_AUDIO": "false", "MEA_AGENT_LISTEN": f"127.0.0.1:{AGENT_PORT}"})
    guacd_log = open(os.path.join(tmp, "guacd.log"), "w")
    libs = ":".join(os.path.join(BUILD, d, ".libs") for d in ("src/protocols/mea", "src/libguac"))
    run(os.path.join(BUILD, "src/guacd/.libs/guacd"), "-f", "-b", "127.0.0.1", "-l", str(GUACD_PORT), "-L", "debug",
        log=guacd_log, env={"LD_LIBRARY_PATH": libs})
    wait(lambda: socket.create_connection(("127.0.0.1", GUACD_PORT), timeout=1).close() or True, 5)
    time.sleep(0.5)

    token = mint()
    g = connect(token)
    seen = pump(g, 8, until=lambda ins, s: s.get("img", 0) + s.get("blob", 0) > 40)
    check(seen.get("ready", 0) == 1 and "_error" not in seen, "guacd connected through the MEA agent", str(seen.get("_error", "")))
    check(seen.get("img", 0) > 5 and seen.get("blob", 0) > 5, "desktop frames drawn", f"img={seen.get('img', 0)} blob={seen.get('blob', 0)}")

    g.send("mouse", 320, 540, 0)
    p = wait(lambda: (lambda q: abs(q["x"] - 320) < 4 and abs(q["y"] - 540) < 4 and q)(probe()), 3) or probe()
    check(abs(p["x"] - 320) < 4 and abs(p["y"] - 540) < 4, "mouse motion reaches X", f"{p['x']},{p['y']}")
    g.send("mouse", 320, 540, 1)
    check(bool(wait(lambda: probe()["buttons"] & 1, 3)), "mouse button reaches X")
    g.send("mouse", 320, 540, 0)
    g.send("key", 0x61, 1)
    check(bool(wait(lambda: 0x61 in probe()["keys"], 3)), "key press reaches X")
    g.send("key", 0x61, 0)
    check(bool(wait(lambda: 0x61 not in probe()["keys"], 3)), "key release reaches X")

    # disconnect while a key is held: the agent must release it
    g.send("key", 0x64, 1)
    wait(lambda: 0x64 in probe()["keys"], 3)
    g.send("disconnect")
    g.s.close()
    check(bool(wait(lambda: 0x64 not in probe()["keys"], 6)), "held key released when guacd disconnects")

    # refused tokens end the connection as CLIENT_UNAUTHORIZED (0x0301 = 769)
    for name, tok in (("replayed", token), ("forged", "eyJ2IjoxfQ.AAAA")):
        g = connect(tok)
        seen = pump(g, 6)
        err = seen.get("_error", [])
        check(err[:1] == ["error"] and err[-1] == "769", f"{name} token refused as unauthorized", str(err))
        g.s.close()
finally:
    for p in procs:
        p.terminate()
    if failures:
        for f in ("guacd.log", "agent.log"):
            print(f"--- {f} ---")
            print(open(os.path.join(tmp, f)).read()[-3000:])
    print(f"{failures} check(s) failed" if failures else "all checks passed")
    sys.exit(1 if failures else 0)
