# common.py
import asyncio
from dataclasses import dataclass, field
from typing import Optional

@dataclass
class Device:
    id: int
    type: str
    address: str
    connected: bool = True
    
    # Przechowujemy referencję do taska czytającego z sieci
    _read_task: Optional[asyncio.Task] = field(default=None, repr=False)
    
    # ... writer i metody send/close pozostają abstrakcyjne/zdefiniowane jak wcześniej ...
    _writer: asyncio.StreamWriter = None 

    async def send_bytes(self, data: bytes):
        raise NotImplementedError

    async def close(self):
        raise NotImplementedError