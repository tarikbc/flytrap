#!/usr/bin/env python3
"""Deploy Flytrap to a Flipper Zero over USB (serial CLI).

Uploads the built fap to /ext/apps/GPIO/ and every portal in ../portals/ to
/ext/apps_data/flytrap/portals/, verifying each file by on-device md5.

Usage: python3 tools/deploy-to-flipper.py --port /dev/cu.usbmodemflip_XXXX
Requires: pyserial
"""
import argparse
import glob
import hashlib
import os
import sys
import time

import serial  # pyserial

PROMPT = b">: "
BLOCK = 4096  # small blocks keep the Flipper's per-write_chunk malloc tiny
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FAP = os.path.join(REPO, "flipper", "flytrap", "dist", "flytrap.fap")
PORTALS = os.path.join(REPO, "portals")


def read_until(s, marker, timeout=8):
    end = time.time() + timeout
    buf = b""
    while time.time() < end:
        n = s.in_waiting
        chunk = s.read(n if n else 1)
        if chunk:
            buf += chunk
            if marker in buf:
                return buf
    return buf


def sync(s):
    s.reset_input_buffer()
    s.write(b"\r")
    read_until(s, PROMPT, timeout=4)
    s.reset_input_buffer()


def cmd(s, command, timeout=8):
    s.write(command.encode() + b"\r")
    return read_until(s, PROMPT, timeout=timeout).decode(errors="replace")


def local_md5(path):
    h = hashlib.md5()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def upload(s, local, remote):
    cmd(s, f"storage remove {remote}")  # write_chunk appends; start clean
    with open(local, "rb") as f:
        while True:
            block = f.read(BLOCK)
            if not block:
                break
            s.reset_input_buffer()
            s.write(f"storage write_chunk {remote} {len(block)}\r".encode())
            if b"Ready" not in read_until(s, b"Ready", timeout=6):
                raise RuntimeError(f"no Ready for {remote}")
            s.write(block)
            read_until(s, PROMPT, timeout=8)
    out = cmd(s, f"storage md5 {remote}")
    return local_md5(local) in out.lower()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", required=True, help="Flipper serial port")
    args = ap.parse_args()

    if not os.path.exists(FAP):
        sys.exit(f"fap not found: {FAP} (build it first: cd flipper/flytrap && ufbt)")

    s = serial.Serial(args.port, timeout=3)
    time.sleep(0.2)
    fails = []
    try:
        sync(s)
        for d in [
            "/ext/apps/GPIO",
            "/ext/apps_data",
            "/ext/apps_data/flytrap",
            "/ext/apps_data/flytrap/portals",
            "/ext/apps_data/flytrap/logs",
        ]:
            cmd(s, f"storage mkdir {d}")

        jobs = [(FAP, "/ext/apps/GPIO/flytrap.fap")]
        for p in sorted(glob.glob(os.path.join(PORTALS, "*.html"))):
            jobs.append((p, f"/ext/apps_data/flytrap/portals/{os.path.basename(p)}"))

        for local, remote in jobs:
            ok = upload(s, local, remote)
            print(f"{'OK  ' if ok else 'FAIL'} {os.path.basename(remote)}")
            if not ok:
                fails.append(remote)
    finally:
        s.close()

    print(f"\n{len(jobs)} files, {len(fails)} failures")
    sys.exit(1 if fails else 0)


if __name__ == "__main__":
    main()
