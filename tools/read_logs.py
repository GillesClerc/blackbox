#!/usr/bin/env python3
"""Reset la box et capture les logs série pendant N secondes.

Usage : python3 tools/read_logs.py [--port /dev/ttyACM0] [--secs 18]
Le monitor live (idf.py monitor) reste sur l'hôte WSL2 ; ceci est pour lire
un boot complet depuis le container (capture one-shot)."""
import argparse
import time

import serial


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default="/dev/ttyACM0")
    ap.add_argument("--secs", type=float, default=18.0)
    args = ap.parse_args()

    s = serial.Serial(args.port, 115200, timeout=0.5)
    # Séquence reset auto (DTR=IO0, RTS=EN) : redémarre dans l'app.
    s.setDTR(False)
    s.setRTS(True)
    time.sleep(0.1)
    s.setRTS(False)

    t = time.time()
    buf = b""
    while time.time() - t < args.secs:
        buf += s.read(4096)
    print(buf.decode("utf-8", "replace"))


if __name__ == "__main__":
    main()
