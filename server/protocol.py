from enum import IntEnum

class FuncCode(IntEnum):
    # System commands (0x1)
    SYS_RAW_SEND = 0x104 #payload: str(any), uart->eth

    # System API commands (0x3)
    SYS_ETH_MSG = 0x301 #payload: str(any), eth->uart
    SYS_LOOP = 0x302 #payload: str(any), eth->eth

    # System Notifications (0xF)
    SYS_NOTIF_UNKNOWN = 0xF01 #payload: None
    SYS_NOTIF_DEBUG   = 0xF02 #payload: str(any)
    SYS_NOTIF_FORBID  = 0xF03 #payload: None
    SYS_NOTIF_OK      = 0xF04 #payload: None


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