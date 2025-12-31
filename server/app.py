from prompt_toolkit import PromptSession
from typing import Optional

from common import Device
from transport import TransportManager
import protocol

class MyApplication:
    def __init__(self, transport_manager: TransportManager):
        self.tm = transport_manager
        self.running = True
        
        self.tm.on_device_connected = self.on_connect
        self.tm.on_device_disconnected = self.on_disconnect
        self.tm.on_data_received = self.on_message

    async def on_connect(self, device: Device):
        print(f"\n[APP] Nowe urządzenie: ID={device.id} Typ={device.type} ({device.address})")

    async def on_disconnect(self, device: Device):
        print(f"[APP] Urządzenie rozłączone: ID={device.id}")

    async def on_message(self, device: Device, packet: bytes):
        
        try:
            length, func_code, flags, msg_str = protocol.parse_packet(packet)
            
            try:
                func_name = protocol.FuncCode(func_code).name
            except ValueError:
                func_name = f"UNKNOWN({func_code})"

            print(f"[APP] Od {device.id}: [{func_name}] Flags={flags} Msg='{msg_str}'")

            response_msg = f"ACK {func_name}"
            response_packet = protocol.build_packet(func_code, 0, response_msg)
            
            await device.send_bytes(response_packet)

        except Exception as e:
            print(f"[APP] Błąd przetwarzania wiadomości od {device.id}: {e}")

    async def run_console(self):
        session = PromptSession()
        print("=== SYSTEM GOTOWY ===")
        print("Komendy: list, send <id> <func> <msg>, exit")

        while self.running:
            try:
                cmd = await session.prompt_async("> ")
                if not cmd: continue
                
                parts = cmd.split()
                command = parts[0].lower()
                match command:
                    case "list":
                        self._handle_list()
                
                    case "send":
                        await self._handle_send(parts)
                    case "disconnect":
                        await self._handle_disconnect(parts)
                    case "exit":
                        self.running = False
                        print("Zamykanie aplikacji...")
                    case "help":
                        print("""
send    Send message to device.
        send <client_id> <func_code> <flags> <data>
disconnect  Disconnect device.
        disconnect <id> | "all"
list    List connected devices.
exit    Stop server.
""")
                    case _:
                        print("Nieznana komenda.")

            except (EOFError, KeyboardInterrupt):
                self.running = False
                break
            except Exception as e:
                print(f"Błąd konsoli: {e}")



    def _handle_list(self):
        print(f"{'ID':<4} {'TYPE':<15} {'ADDRESS':<20} {'STATUS'}")
        print("-" * 50)
        if not self.tm.devices:
            print("(Brak połączonych urządzeń)")
        else:
            for dev in self.tm.devices.values():
                status = "Connected" if dev.connected else "Zombie"
                print(f"{dev.id:<4} {dev.type:<15} {dev.address:<20} {status}")

    async def _handle_send(self, parts):
        if len(parts) < 4:
            print("Użycie: send <id> <func_code> <message>")
            return

        try:
            target_id = int(parts[1])
            func_input = parts[2]
            message = " ".join(parts[3:])
   
            if func_input.isdigit():
                func_val = int(func_input)
            else:
                try:
                    func_val = protocol.FuncCode[func_input.upper()].value
                except KeyError:
                    print(f"Nieznany kod funkcji: {func_input}")
                    print(f"Dostępne: {[f.name for f in protocol.FuncCode]}")
                    return

            device = self.tm.devices.get(target_id)
            if not device:
                print(f"Błąd: Nie ma urządzenia o ID {target_id}")
                return

            packet = protocol.build_packet(func_val, 0, message)
            await device.send_bytes(packet)
            print(f"✅ Wysłano do {device.type} (ID: {target_id})")

        except ValueError:
            print("Błąd: ID musi być liczbą.")
        except Exception as e:
            print(f"Błąd wysyłania: {e}")
            
    async def _handle_disconnect(self, parts):
        if len(parts) <2:
            print("Użycie: disconnect <id> | \"all\"")
            return
        try:
            if parts[1] == "all":
                await self.tm.disconnect_all()
                return
            target_id = int(parts[1])
                        
            if not await self.tm.disconnect(target_id):
                print(f"Błąd: Nie ma urządzenia o ID {target_id}")
                return
            print(f"Zamknięto urządzenie (ID: {target_id})")

        except ValueError:
            print("Błąd: ID musi być liczbą.")
        except Exception as e:
            print(f"Błąd zamknęcia połączenia: {e}")
        