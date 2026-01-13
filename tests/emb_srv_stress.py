#!/usr/bin/env python3
import socket
import time
import argparse
import sys
import random

def main():
    parser = argparse.ArgumentParser(description="Single-connection TCP stress tester")
    parser.add_argument("ip", help="Target device IP")
    parser.add_argument("--port", type=int, default=5000)
    parser.add_argument("--hold", type=float, default=0.2, help="Seconds to keep connection open")
    parser.add_argument("--delay", type=float, default=0.05, help="Delay between cycles")
    parser.add_argument("--loops", type=int, default=0, help="0 = run forever")
    parser.add_argument("--abrupt", action="store_true", help="Close without FIN")
    args = parser.parse_args()

    count = 0
    while True:
        count += 1
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.settimeout(3)

            start = time.time()
            s.connect((args.ip, args.port))
            print(f"[{count}] CONNECTED in {time.time()-start:.3f}s")

            # Optional tiny payload
            s.sendall(b"PING\n")

            time.sleep(args.hold)

            if args.abrupt:
                s.shutdown(socket.SHUT_RDWR)
                s.close()
                print(f"[{count}] ABRUPT CLOSE")
            else:
                s.close()
                print(f"[{count}] CLEAN CLOSE")

        except Exception as e:
            print(f"[{count}] ERROR: {e}")
            time.sleep(1)

        time.sleep(args.delay)

        if args.loops and count >= args.loops:
            break

if __name__ == "__main__":
    main()

