import pytest

from server.protocol import parse_packet, build_packet, FuncCode
@pytest.fixture()
def inputs():
    message= "abcdef"
    data = message.encode('ascii')
    length = len(data) +4
    func_code = 0x000
    flags = 0x00
    header_int = (length & 0xFFF) << 20
    header_int |= (func_code & 0xFFF) << 8
    header_int |= (flags & 0xFF)
    
    return header_int.to_bytes(4, 'big') + data , length, func_code, flags, message

class TestParsePacket():
    
    def test_correct(self, inputs):
        bytess , length, func_code, flags, message = inputs
        
        assert parse_packet(bytess) == (length, func_code, flags, message)
        
        

class TestBuildPacket():
    
    def test_correct(self,inputs):
        
        bytess , length, func_code, flags, message = inputs
        assert build_packet(func_code, flags, message) == bytess