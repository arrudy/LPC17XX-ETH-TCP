#!/usr/bin/env python3
"""
MCU TCP duplex test - Passive Responder Version
"""

import socket
import struct
import argparse
import random
import string

# Constants
CMD_PING      = 0x104
CMD_RESPONSE  = 0x301
FLAGS         = 0
HEADER_SIZE   = 4

# ---------- Protocol helpers ----------

def format_packet_info(length, func, flags, payload):
    """Returns a string representing the packet with fields in Hex."""
    return f"[Len: {length}, Func: 0x{func:03X}, Flags: 0x{flags:02X}] Payload: '{payload}'"

def pack_packet(payload_string: str, func_code: int) -> bytes:
    """Pack a header and string payload."""
    payload = payload_string.encode('ascii')
    total_len = HEADER_SIZE + len(payload)
    
    # 12 bits length | 12 bits func | 8 bits flags
    header_int = ((total_len & 0xFFF) << 20) | ((func_code & 0xFFF) << 8) | (FLAGS & 0xFF)
    
    return struct.pack(f">I{len(payload)}s", header_int, payload)

def unpack_packet(data: bytes):
    """Unpack header and payload, returning (length, func, flags, payload_str)."""
    if len(data) < 4:
        return None
    
    header_int = struct.unpack(">I", data[:4])[0]
    length = (header_int >> 20) & 0xFFF
    func   = (header_int >> 8)  & 0xFFF
    flags  = header_int & 0xFF
    
    # Extract payload based on the length field in the header
    payload = data[4:length].decode('ascii')
    
    return length, func, flags, payload

def generate_random_string(length=16):
    """Generate a string of random ASCII characters."""
    chars = string.ascii_letters + string.digits
    return ''.join(random.choice(chars) for _ in range(length))

def handle_request(data):
    """Common logic: Inspect RX data and determine the Response string."""
    unpacked = unpack_packet(data)
    if not unpacked:
        return None, None
    
    length, func, flags, payload = unpacked
    print(f"  RX: {format_packet_info(length, func, flags, payload)}")

    # Logical Check: Respond to 0x104 "ping" with 0x301 "pong"
    if func == CMD_PING and payload == "ping\0":
        response_str = "pong"
    else:
        response_str = generate_random_string(16)
    
    return response_str, CMD_RESPONSE

# ---------- Server Mode ----------

def run_server(host, port):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        s.bind((host, port))
        s.listen(1)
        print(f"SERVER: Listening on {host}:{port}. Waiting for external request...")

        conn, addr = s.accept()
        with conn:
            print(f"SERVER: Connected by {addr}")
            while True:
                data = conn.recv(1024)
                if not data:
                    print("SERVER: Connection closed by peer.")
                    break

                resp_payload, resp_cmd = handle_request(data)
                if resp_payload:
                    pkt = pack_packet(resp_payload, resp_cmd)
                    conn.sendall(pkt)
                    # Log the transmitted packet in Hex
                    l, fu, fl, p = unpack_packet(pkt)
                    print(f"  TX: {format_packet_info(l, fu, fl, p)}")

# ---------- Client Mode ----------

def run_client(host, port):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        print(f"CLIENT: Connecting to {host}:{port}...")
        s.connect((host, port))
        print(f"CLIENT: Connected. Waiting for external request (Passive Mode)...")

        while True:
            data = s.recv(1024)
            if not data:
                print("CLIENT: Connection closed by peer.")
                break

            resp_payload, resp_cmd = handle_request(data)
            if resp_payload:
                pkt = pack_packet(resp_payload, resp_cmd)
                s.sendall(pkt)
                # Log the transmitted packet in Hex
                l, fu, fl, p = unpack_packet(pkt)
                print(f"  TX: {format_packet_info(l, fu, fl, p)}")

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
