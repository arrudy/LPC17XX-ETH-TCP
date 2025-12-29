from enum import IntEnum

class FuncCode(IntEnum):
    CAT_SIM_API = 1
    CAT_SIM_NOTIF = 2
    CAT_SYS_NOTIF = 3


def parse_packet(packet: bytes):
    """Rozbija surowe bajty na czytelne składowe."""
    header = packet[:4]
    payload = packet[4:]
    
    val = int.from_bytes(header, 'big')
    length = (val >> 20) & 0xFFF
    func_code = (val >> 8) & 0xFFF
    flags = val & 0xFF
    
    msg_str = payload.decode('utf-8', errors='replace')
    
    return length, func_code, flags, msg_str

def build_packet(func_code: int, flags: int, message: str) -> bytes:
    """Tworzy bajty gotowe do wysłania."""
    data = message.encode('utf-8')
    length = len(data)
    
    header_int = (length & 0xFFF) << 20
    header_int |= (func_code & 0xFFF) << 8
    header_int |= (flags & 0xFF)
    
    return header_int.to_bytes(4, 'big') + data