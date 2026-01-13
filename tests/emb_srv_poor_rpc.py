#!/usr/bin/env python3
"""
MCU TCP duplex test - Periodic Sender Version
Sends a packet once per second, regardless of response.
"""

import socket
import struct
import argparse
import time

FUNC_CODE = 0x101
FLAGS     = 0
HEADER_SIZE = 4
TARGET_IP = "10.0.209.26\0"

# ---------- Protocol helpers ----------

def pack_packet(ip_string: str) -> bytes:
    payload = ip_string.encode('ascii')
    total_len = HEADER_SIZE + len(payload)
    
    # 12 bits length | 12 bits func | 8 bits flags
    header_int = ((total_len & 0xFFF) << 20) | ((FUNC_CODE & 0xFFF) << 8) | (FLAGS & 0xFF)
    
    return struct.pack(f">I{len(payload)}s", header_int, payload)

def unpack_packet(data: bytes):
    if len(data) < 4:
        return 0, 0, 0, ""
    
    header_int = struct.unpack(">I", data[:4])[0]
    length = (header_int >> 20) & 0xFFF
    func   = (header_int >> 8)  & 0xFFF
    flags  = header_int & 0xFF
    
    payload = data[4:length].decode('ascii') if len(data) >= length else ""
    return length, func, flags, payload

# ---------- Server ----------

def run_server(host, port):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        s.bind((host, port))
        s.listen(1)
        print(f"Server listening on {host}:{port}")

        conn, addr = s.accept()
        with conn:
            print("Server connected by", addr)
            
            # Optional: Set non-blocking if you want to drain buffer without waiting
            # conn.setblocking(False) 

            try:
                while True:
                    pkt = pack_packet(TARGET_IP)
                    conn.sendall(pkt)
                    
                    # Print what we just sent
                    print(f"Server TX: {unpack_packet(pkt)} | Time: {time.time():.2f}")
                    
                    # Wait 1 second before next packet
                    time.sleep(1)
                    
            except (BrokenPipeError, ConnectionResetError):
                print("Client disconnected.")

# ---------- Client ----------

def run_client(host, port):
    while True:
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
                print(f"Connecting to {host}:{port}...")
                s.connect((host, port))
                print(f"Client connected.")

                while True:
                    pkt = pack_packet(TARGET_IP)
                    s.sendall(pkt)
                    
                    # Print what we just sent
                    print(f"Client TX: {unpack_packet(pkt)} | Time: {time.time():.2f}")
                    
                    # Wait 1 second before next packet
                    time.sleep(1)

        except (ConnectionRefusedError, TimeoutError):
            print("Connection failed. Retrying in 2 seconds...")
            time.sleep(2)
        except (BrokenPipeError, ConnectionResetError):
            print("Connection lost. Reconnecting...")
            time.sleep(1)
        except KeyboardInterrupt:
            print("\nStopping...")
            break

# ---------- Main ----------

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--mode", choices=["server", "client"], required=True)
    parser.add_argument("--host", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=5000)
    args = parser.parse_args()

    if args.mode == "server":
        run_server(args.host, args.port)
    else:
        run_client(args.host, args.port)