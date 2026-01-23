from prompt_toolkit import PromptSession
import re, os , random

from common import Device
from transport import TransportManager
import protocol
from usecases import func_runner

ip_pattern = re.compile(r"^(\d{1,3}\.){3}\d{1,3}$")

class MyApplication:
    def __init__(self, transport_manager: TransportManager):
        self.tm = transport_manager
        self.running = True
        self.spam_targets = set()
        self.spam_task = None

        self.tm.on_device_connected = self.on_connect
        self.tm.on_device_disconnected = self.on_disconnect
        self.tm.on_data_received = self.on_message

    async def on_connect(self, device: Device):
        print(
            f"\n[APP] Nowe urządzenie: ID={device.id} Typ={device.type} ({device.address})"
        )

    async def on_disconnect(self, device: Device):
        print(f"[APP] Urządzenie rozłączone: ID={device.id}")

    async def on_message(self, device: Device, packet: bytes):

        try:
            length, func_code, flags, msg_str = protocol.parse_packet(packet)

            try:
                func_name = protocol.FuncCode(func_code).name
            except ValueError:
                func_name = f"UNKNOWN({func_code})"

            print(
                f"[APP] Od {device.id}: [{func_name}] Le={length} Flags={flags} Msg='{msg_str}'"
            )
            
            send = func_runner(msg_str.lower())
            if send:
                packet = protocol.build_packet(0x301, 0, send)
                await device.send_bytes(packet)

        except Exception as e:
            print(f"[APP] Błąd przetwarzania wiadomości od {device.id}: {e}")

    async def run_console(self):
        session = PromptSession()

        while self.running:
            try:
                cmd = await session.prompt_async("> ")
                if not cmd:
                    continue

                parts = cmd.split()
                command = parts[0].lower()
                match command:

                    case "send":
                        await self._handle_send(parts[1:])
                    case "connect":
                        await self._handle_connect(parts[1:])
                    case "disconnect":
                        await self._handle_disconnect(parts[1:])
                    case "list":
                        self._handle_list()                        
                    case "hi":
                        await self._handle_hi()
                    case "spam":
                        await self._handle_spam(parts[1:])
                    case "exit":
                        self.running = False
                        print("Zamykanie aplikacji...")
                    case "help":
                        print(
                            """
                            send    Send message to device.
                                    send <client_id> <func_code> <flags> <data>
                            connect Connect to device.
                                    connect <ip> | <address>
                            disconnect  Disconnect device.
                                    disconnect <id> | "all"
                            list    List connected devices.
                            hi      Send hi message to all devices.
                            spam    Send spam messages to all devices.
                                    spam <id> | "all" | "stop"
                            exit    Stop server.
                            """
                        )
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
        if len(parts) < 2:
            print("Użycie: send <id> <func_code> <message>")
            return

        try:
            target_id = int(parts[0])
            func_input = parts[1]
            message = " ".join(parts[2:])

            if func_input.isdigit():
                func_val = int(func_input)
            else:
                try:
                    func_val = protocol.FuncCode[func_input.upper()].value
                except KeyError:
                    print(f"Nieznany kod funkcji: {func_input}")
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
        if len(parts) < 1:
            print('Użycie: disconnect <id> | "all"')
            return
        try:
            if parts[0] == "all":
                await self.tm.disconnect_all()
                return
            target_id = int(parts[0])

            if not await self.tm.disconnect(target_id):
                print(f"Błąd: Nie ma urządzenia o ID {target_id}")
                return
            print(f"Zamknięto urządzenie (ID: {target_id})")

        except ValueError:
            print("Błąd: ID musi być liczbą.")
        except Exception as e:
            print(f"Błąd zamknęcia połączenia: {e}")

    async def _handle_connect(self, parts):
        if len(parts) < 1:
            print("Użycie: connect <ip> | <address>")
            return

        for address in parts:
            await self.tm.connect(address)

    async def _handle_hi(self):
        print("Wysyłam HI do wszystkich urządzeń...")

        for device in self.tm.devices.values():
            try:
                packet = protocol.build_packet(0x302, 0, "HI")
                await device.send_bytes(packet)
                print(f"✅ Wysłano HI do {device.type} (ID: {device.id})")
            except Exception as e:
                print(f"Błąd wysyłania HI do {device.id}: {e}")
    
            
    async def _handle_spam(self, parts):

        cmd = parts[0].lower()

        if cmd == "stop":
            await self.tm.stop_spam_job()
            return


        targets = []
        if cmd == "all":

            targets = list(self.tm.devices.values())
        else:

            for raw_id in parts[0:]:
                try:
                    tid = int(raw_id)
                    dev = self.tm.devices.get(tid)
                    if dev: targets.append(dev)
                    else: print(f"⚠️ Brak urządzenia ID: {tid}")
                except ValueError: pass

        if not targets:
            print("⚠️ Brak poprawnych celów do spamowania.")
            return
        
        filename = "spam.txt"
        if not os.path.exists(filename):
            print(f"❌ Błąd: Nie znaleziono pliku '{filename}'")
            return

        try:
            with open(filename, "r", encoding="ascii") as f:
               
                lines = [line.strip() for line in f if line.strip()]
            
            if not lines:
                print("⚠️ Plik jest pusty!")
                return
                

        except Exception as e:
            print(f"❌ Błąd odczytu pliku: {e}")
            return

        def make_spam_packet() -> bytes:
            text = random.choice(lines)
            
            return protocol.build_packet(0x301, 0, text)

        await self.tm.start_spam_job(targets, make_spam_packet)