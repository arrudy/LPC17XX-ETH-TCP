import asyncio
import re
import struct
import serial_asyncio
from typing import Dict, Optional, List, Callable

from common import Device
from adapters import TcpDevice, UartDevice, RadioDevice

ip_pattern = re.compile(r"^(\d{1,3}\.){3}\d{1,3}$")


class TransportManager:
    def __init__(self):
        self.devices: Dict[int, Device] = {}
        self._id_counter = 0
        self._lock = asyncio.Lock()
        self._running = True
        self.gateway_tasks = []
        self._spam_task: Optional[asyncio.Task] = None
        self._spam_running = False

        self.on_device_connected = None
        self.on_device_disconnected = None
        self.on_data_received = None

    async def start_tcp(self, host: str, port: int):
        async def cb(reader, writer):
            addr = writer.get_extra_info("peername")
            async with self._lock:
                self._id_counter += 1
                dev = TcpDevice(
                    id=self._id_counter,
                    type="TCP",
                    address=f"{addr[0]}:{addr[1]}",
                    _writer=writer,
                )
                self.devices[dev.id] = dev

            if self.on_device_connected:
                await self.on_device_connected(dev)

            dev._read_task = asyncio.create_task(self._generic_loop(dev, reader))

        server = await asyncio.start_server(cb, host, port)
        print(f"✅ [Transport] TCP Server na {host}:{port}")
        return server

    async def start_direct_uart(self, port: str, baud: int, label="UART"):
        try:
            reader, writer = await serial_asyncio.open_serial_connection(
                url=port, baudrate=baud
            )
            print(f"✅ [Transport] {label} na {port}")

            async with self._lock:
                self._id_counter += 1
                dev = UartDevice(
                    id=self._id_counter, 
                    type=label, 
                    address=port, 
                    _writer=writer
                )
                self.devices[dev.id] = dev

            if self.on_device_connected:
                await self.on_device_connected(dev)

            dev._read_task = asyncio.create_task(self._generic_loop(dev, reader))

        except Exception as e:
            print(f"[Transport] Błąd UART {port}: {e}")

    async def start_radio_gateway(self, port: str, baud: int, label="RADIO"):
        try:
            reader, writer = await serial_asyncio.open_serial_connection(
                url=port, baudrate=baud
            )
            print(f"✅ [Transport] {label} Gateway na {port}")
            gw_lock = asyncio.Lock()

            task = asyncio.create_task(self._radio_loop(reader, writer, gw_lock, label))
            self.gateway_tasks.append(task)
        except Exception as e:
            print(f"[Transport] Błąd RADIO {port}: {e}")
    
    async def start_spam_job(self, targets: List[Device], packet_factory: Callable[[int], bytes]):

        await self.stop_spam_job()
        
        self._spam_running = True
        self._spam_task = asyncio.create_task(self._spam_loop(targets, packet_factory))
        print(f"🚀 [Transport] Uruchomiono SPAM task dla {len(targets)} urządzeń.")

    async def stop_spam_job(self):

        if self._spam_task:
            self._spam_running = False
            self._spam_task.cancel()
            try:
                await self._spam_task
            except asyncio.CancelledError:
                pass
            self._spam_task = None
            print("🛑 [Transport] SPAM task zatrzymany.")
    
    async def send_to_device(self, device_id: int, data: bytes):
        async with self._lock:
            device = self.devices.get(device_id)
        if device:
            await device.send_bytes(data)
    
    async def connect(self, address):

        if ip_pattern.match(address):
            await self._connect_tcp(address)

        elif address.upper().startswith("COM"):
            await self.start_direct_uart(address, 115200)

        elif address.startswith("/dev/"):
            await self.start_direct_uart(address, 115200)

        else:
            raise ValueError
        print(f"Połączono urządzenie (ID: {address})")

    async def disconnect(self, id: int):
        device = None
        async with self._lock:
            if id in self.devices.keys():
                device = self.devices[id]
                del self.devices[id]

        if device:
            await device.close()
            if self.on_device_disconnected:
                await self.on_device_disconnected(device)
            return True
        return False

    async def disconnect_all(self):
        async with self._lock:
            active_devices = list(self.devices.values())
        for dev in active_devices:
            await dev.close()

    async def shutdown(self):
        print("[Transport] Wymuszanie zatrzymania...")
        self._running = False

        for t in self.gateway_tasks:
            t.cancel()

        
        async with self._lock:
            devices_to_kill = list(self.devices.values())
            self.devices.clear()

        if devices_to_kill:
            shutdown_coros = [dev.close() for dev in devices_to_kill]
            
            try:
                await asyncio.wait_for(
                    asyncio.gather(*shutdown_coros, return_exceptions=True), timeout=2.0
                )
            except asyncio.TimeoutError:
                print(
                    "[Transport] Shutdown timeout - niektóre połączenia mogły zostać zerwane siłowo."
                )

        if self.gateway_tasks:
            try:
                await asyncio.wait_for(
                    asyncio.gather(*self.gateway_tasks, return_exceptions=True),
                    timeout=1.0,
                )
            except asyncio.TimeoutError:
                pass

        print("[Transport] Manager wyłączony.")

    async def _handle_disconnect(self, device):

        async with self._lock:
            if device.id in self.devices:
                del self.devices[device.id]
                await self._notify_disconnect(device)

        try:
            device._writer.close()
        except:
            pass

    async def _notify_disconnect(self, device):
        if self.on_device_disconnected:
            await self.on_device_disconnected(device)

    async def _get_radio_device(self, radio_id, label, writer, lock):
        # (Bez zmian)
        async with self._lock:
            if radio_id in self.devices:
                return self.devices[radio_id]
            dev = RadioDevice(
                radio_id,
                f"{label}_NODE",
                f"RF:{radio_id}",
                _gateway_writer=writer,
                _gateway_lock=lock,
            )
            self.devices[radio_id] = dev
        if self.on_device_connected:
            await self.on_device_connected(dev)
        return dev

    async def _connect_tcp(self, host: str, port: int = 5000) -> Optional[Device]:
        try:
            reader, writer = await asyncio.wait_for(
                asyncio.open_connection(host, port), timeout=5.0
            )

            addr_str = f"{host}:{port}"

            async with self._lock:
                self._id_counter += 1
                dev = TcpDevice(
                    id=self._id_counter,
                    type="TCP_CLIENT", 
                    address=addr_str,
                    _writer=writer,
                )
                self.devices[dev.id] = dev

            print(f"[Transport] Połączono z {host}:{port} (ID: {dev.id})")

            if self.on_device_connected:
                await self.on_device_connected(dev)

            dev._read_task = asyncio.create_task(self._generic_loop(dev, reader))

            return dev

        except asyncio.TimeoutError:
            print(f"[Transport] Timeout połączenia do {host}:{port}")
            return None
        except ConnectionRefusedError:
            print(
                f"[Transport] Odrzucono połączenie do {host}:{port} (Serwer nie działa?)"
            )
            return None
        except Exception as e:
            print(f"[Transport] Błąd łączenia z {host}:{port}: {e}")
            return None

    async def _generic_loop(self, device: Device, reader: asyncio.StreamReader):
        try:
            while self._running:
                header = await reader.readexactly(4)
                h_val = int.from_bytes(header, "big")
                length = (h_val >> 20) & 0xFFF
                payload = await reader.readexactly(length - 4)

                if self.on_data_received:
                    await self.on_data_received(device, header + payload)

        except asyncio.CancelledError:
            pass
        except (asyncio.IncompleteReadError, Exception):
            pass
        finally:
            await self._handle_disconnect(device)

    async def _radio_loop(self, reader, writer, lock, label):
        try:
            while self._running:
                wrapper = await reader.readexactly(4)
                length, source_id = struct.unpack(">HH", wrapper)
                packet = await reader.readexactly(length - 4)

                dev = await self._get_radio_device(source_id, label, writer, lock)

                if self.on_data_received:
                    await self.on_data_received(dev, packet)
        except asyncio.CancelledError:
            pass
        except Exception:
            pass

    
    
    async def _spam_loop(self, targets: List[Device], packet_factory: Callable[[int], bytes]):
        counter = 0
       
        active_targets = list(targets) 
        
        try:
            while self._spam_running and active_targets:
                counter += 1
                
                packet = packet_factory()
                
                active_targets = [d for d in active_targets if d.connected]
                
                if not active_targets:
                    print("🏁 [Transport] Wszyscy odbiorcy spamu rozłączeni.")
                    break

                send_coroutines = [dev.send_bytes(packet) for dev in active_targets]
                
       
                await asyncio.gather(*send_coroutines, return_exceptions=True)
                await asyncio.sleep(1.0)
                
        except asyncio.CancelledError:
            pass
        except Exception as e:
            print(f"❌ [Transport] Błąd pętli spamującej: {e}")
        finally:
            self._spam_running = False
            self._spam_task = None
