import asyncio
from dataclasses import dataclass, field
from typing import Optional


@dataclass
class Device:
    id: int
    type: str
    address: str
    connected: bool = True
    _energy: int = 0
    _read_task: Optional[asyncio.Task] = field(default=None, repr=False)

    _writer: asyncio.StreamWriter = None

    async def send_bytes(self, data: bytes):
        raise NotImplementedError

    async def close(self):
        raise NotImplementedError
    
    def refill_energy(self, amount: int):
        self._energy += amount
        
    def get_energy(self) -> int:
        return self._energy