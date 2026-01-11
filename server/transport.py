import asyncio
import struct
import serial_asyncio
from typing import Dict

from common import Device
from adapters import TcpDevice, UartDevice, RadioDevice


class TransportManager:
    def __init__(self):
        self.devices: Dict[int, Device] = {}
        self._id_counter = 0
        self._lock = asyncio.Lock()
        self._running = True
        self.gateway_tasks = [] 
        
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
                    _writer=writer
                )
                self.devices[dev.id] = dev
            
            if self.on_device_connected: await self.on_device_connected(dev)
            
            dev._read_task = asyncio.create_task(self._generic_loop(dev, reader))

        server = await asyncio.start_server(cb, host, port)
        print(f"✅ [Transport] TCP Server na {host}:{port}")
        return server

    async def start_direct_uart(self, port: str, baud: int, label="UART"):
        try:
            reader, writer = await serial_asyncio.open_serial_connection(url=port, baudrate=baud)
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

            if self.on_device_connected: await self.on_device_connected(dev)
            
            dev._read_task = asyncio.create_task(self._generic_loop(dev, reader))
            
        except Exception as e:
            print(f"[Transport] Błąd UART {port}: {e}")

    async def start_radio_gateway(self, port: str, baud: int, label="RADIO"):
        try:
            reader, writer = await serial_asyncio.open_serial_connection(url=port, baudrate=baud)
            print(f"✅ [Transport] {label} Gateway na {port}")
            gw_lock = asyncio.Lock()
            
            task = asyncio.create_task(self._radio_loop(reader, writer, gw_lock, label))
            self.gateway_tasks.append(task)
        except Exception as e:
            print(f"[Transport] Błąd RADIO {port}: {e}")

    async def _generic_loop(self, device: Device, reader: asyncio.StreamReader):
        try:
            while self._running:
                header = await reader.readexactly(4)
                h_val = int.from_bytes(header, 'big')
                length = (h_val >> 20) & 0xFFF
                payload = await reader.readexactly(length-4)
                
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
                length, source_id = struct.unpack('>HH', wrapper)
                packet = await reader.readexactly(length-4)
            
                dev = await self._get_radio_device(source_id, label, writer, lock)
                
                if self.on_data_received:
                    await self.on_data_received(dev, packet)
        except asyncio.CancelledError:
            pass
        except Exception:
            pass
    
    async def send_to_device(self, device_id: int, data: bytes):
        async with self._lock:
            device = self.devices.get(device_id)
        if device:
            await device.send_bytes(data)
    

    async def disconnect(self, id: int):
        device = None
        async with self._lock:
            if id in self.devices:
                device = self.devices[id]
                del self.devices[id] # Usuwamy referencję od razu
        
        if device:
            await device.close() # To anuluje taska i zamknie writer
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
        
        for t in self.gateway_tasks: t.cancel()
        
        # 2. Pobierz listę urządzeń i od razu wyczyść słownik (żeby uniknąć iterowania po zmieniającym się obiekcie)
        async with self._lock:
            devices_to_kill = list(self.devices.values())
            self.devices.clear()

        # 3. Zabij wszystkie urządzenia RÓWNOLEGLE z timeoutem
        # gather() uruchomi close() dla wszystkich naraz.
        if devices_to_kill:
            shutdown_coros = [dev.close() for dev in devices_to_kill]
            
            # WAŻNE: wait_for na wszystkim naraz. 
            # Jeśli cokolwiek się zawiesi, utniemy to po 2 sekundach.
            try:
                await asyncio.wait_for(asyncio.gather(*shutdown_coros, return_exceptions=True), timeout=2.0)
            except asyncio.TimeoutError:
                print("[Transport] Shutdown timeout - niektóre połączenia mogły zostać zerwane siłowo.")

        # 4. Poczekaj na gatewaye
        if self.gateway_tasks:
            try:
                await asyncio.wait_for(asyncio.gather(*self.gateway_tasks, return_exceptions=True), timeout=1.0)
            except asyncio.TimeoutError: pass

        print("[Transport] Manager wyłączony.")

    async def _handle_disconnect(self, device):

        async with self._lock:
            if device.id in self.devices:
                del self.devices[device.id]
                await self._notify_disconnect(device)
        

        try:
            device._writer.close()
        except: pass

    async def _notify_disconnect(self, device):
        if self.on_device_disconnected:
            await self.on_device_disconnected(device)
    
    async def _get_radio_device(self, radio_id, label, writer, lock):
        # (Bez zmian)
        async with self._lock:
            if radio_id in self.devices: return self.devices[radio_id]
            dev = RadioDevice(radio_id, f"{label}_NODE", f"RF:{radio_id}", _gateway_writer=writer, _gateway_lock=lock)
            self.devices[radio_id] = dev
        if self.on_device_connected: await self.on_device_connected(dev)
        return dev
    
    async def connect(self, address):
        if address == "all":
            await self.connect_all()
            
        elif ip_pattern.match(address):
            await self._connect_via_tcp(address)
        
        
        elif address.upper().startswith("COM"):
            await self._connect_via_serial(address, "Windows")
            
        # 3. Sprawdzenie czy to Linux /dev/
        elif address.startswith("/dev/"):
            await self._connect_via_serial(address, "Linux")
            
        # 4. Obsługa błędów
        else:
            wrong_addr = address
            raise ValueError
        print(f"Połączono urządzenie (ID: {address})")
        
    async def _connect_tcp(self, host: str, port: int=5000) -> Optional[Device]:
        "
        try:
          
            reader, writer = await asyncio.wait_for(
                asyncio.open_connection(host, port), 
                timeout=5.0
            )
            
            
            addr_str = f"{host}:{port}"
            
            async with self._lock:
                self._id_counter += 1
                dev = TcpDevice(
                    id=self._id_counter,
                    type="TCP_CLIENT",  # Możemy oznaczyć, że to my zadzwoniliśmy
                    address=addr_str,
                    _writer=writer
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
            print(f"[Transport] Odrzucono połączenie do {host}:{port} (Serwer nie działa?)")
            return None
        except Exception as e:
            print(f"[Transport] Błąd łączenia z {host}:{port}: {e}")
            return None