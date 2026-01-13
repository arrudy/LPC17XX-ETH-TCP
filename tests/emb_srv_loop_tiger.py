#!/usr/bin/env python3
"""
MCU TCP duplex test - String Payload Version
"""

import socket
import struct
import argparse

FUNC_CODE = 0x302  # Updated to match your docstring
FLAGS     = 0
HEADER_SIZE = 4
TARGET_IP = "10.0.209.26\0"

# ---------- Protocol helpers ----------

def pack_packet(ip_string: str) -> bytes:
    # Convert string to bytes
    payload = ip_string.encode('ascii')
    
    # Calculate total length: 4 bytes (header) + length of payload
    total_len = HEADER_SIZE + len(payload)
    
    # Construct 32-bit header
    # 12 bits length | 12 bits func | 8 bits flags
    header_int = ((total_len & 0xFFF) << 20) | ((FUNC_CODE & 0xFFF) << 8) | (FLAGS & 0xFF)
    
    # Pack header (Big Endian) followed by the string bytes
    # Format: ">I" (4-byte unsigned int) + "s" (string of N length)
    return struct.pack(f">I{len(payload)}s", header_int, payload)

def unpack_packet(data: bytes):
    # Peek at the first 4 bytes to get the header
    header_int = struct.unpack(">I", data[:4])[0]
    
    length = (header_int >> 20) & 0xFFF
    func   = (header_int >> 8)  & 0xFFF
    flags  = header_int & 0xFF
    
    # The rest is the string data
    payload = data[4:length].decode('ascii')
    
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

            # Send the IP string immediately
            pkt = pack_packet(TARGET_IP)
            conn.sendall(pkt)
            print("Server TX:", unpack_packet(pkt))

            while True:
                # Note: For variable length, you'd usually read 4 bytes first
                # then read the remaining (length - 4) bytes. 
                # For this test, we read a buffer.
                data = conn.recv(1024)
                if not data:
                    break

                print("Server RX:", unpack_packet(data))
                
                pkt = pack_packet(TARGET_IP)
                conn.sendall(pkt)

# ---------- Client ----------

def run_client(host, port):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.connect((host, port))
        print(f"Client connected to {host}:{port}")

        # Send IP string immediately
        pkt = pack_packet(TARGET_IP)
        s.sendall(pkt)
        print("Client TX:", unpack_packet(pkt))

        while True:
            data = s.recv(1024)
            if not data:
                break

            print("Client RX:", unpack_packet(data))
            
            pkt = pack_packet(TARGET_IP)
            s.sendall(pkt)

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
