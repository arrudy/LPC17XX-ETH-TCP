import asyncio
from dataclasses import dataclass
import struct

from common import Device

@dataclass
class Uart_TcpDevice(Device):
    async def send_bytes(self, data: bytes):
        if not self.connected: raise ConnectionError("Disconnected")
        self._writer.write(data)
        await self._writer.drain()

    async def close(self):
        self.connected = False
        if self._read_task:
            self._read_task.cancel()
        try:
            self._writer.close()
            await asyncio.wait_for(self._writer.wait_closed(), timeout=0.5)
        except (asyncio.TimeoutError, asyncio.CancelledError, Exception):
                pass

@dataclass
class TcpDevice(Uart_TcpDevice):
    pass

@dataclass
class UartDevice(Uart_TcpDevice):
    pass

@dataclass
class RadioDevice(Device):
    _gateway_writer: asyncio.StreamWriter = None
    _gateway_lock: asyncio.Lock = None

    async def send_bytes(self, data: bytes):
        if not self.connected: raise ConnectionError("Disconnected")
        header = struct.pack('>HH', len(data), self.id)
        async with self._gateway_lock:
            self._gateway_writer.write(header + data)
            await self._gateway_writer.drain()

    async def close(self):
        self.connected = False
        if self._read_task:
            self._read_task.cancel()
        try:
            self._writer.close()
            await asyncio.wait_for(self._writer.wait_closed(), timeout=0.5)
        except (asyncio.TimeoutError, asyncio.CancelledError, Exception):
                pass
    