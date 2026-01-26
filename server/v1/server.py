import asyncio
import sys
from dataclasses import dataclass, field
from typing import Dict
from prompt_toolkit import PromptSession
from enum import IntEnum

HOST = '127.0.0.1'
PORT = 5000
BUFFER_SIZE = 2048


class FuncCode(IntEnum):
    CAT_SIM_API = 1
    CAT_SIM_NOTIF = 2
    CAT_SYS_NOTIF = 3

@dataclass
class ClientInfo:
    addr: tuple
    writer: asyncio.StreamWriter
    last_msg: str = ""
    connected: bool = True


@dataclass
class AppState:
    clients: Dict[int, ClientInfo] = field(default_factory=dict)
    running = asyncio.Event()

    client_id_counter: int = 0
    clients_lock = asyncio.Lock()


def extract_header(header):
    header_int = int.from_bytes(header, byteorder='big')
    mask_12_bit = (1 << 12) - 1
    mask_8_bit = (1 << 8) - 1

    length = (header_int >> 20) & mask_12_bit
    function_code = (header_int >> 8) & mask_12_bit
    flags = header_int & mask_8_bit

    return length, function_code, flags


def build_header(length: int, func_code: int, flags: int):
    header_int = (length & 0xFFF) << 20
    header_int |= (func_code & 0xFFF) << 8
    header_int |= (flags & 0xFF)
    return header_int.to_bytes(4, byteorder='big')


async def console(state: AppState):
    session = PromptSession()
    while not state.running.is_set():
        try:
            cmd = await session.prompt_async("> ")
        except (EOFError, KeyboardInterrupt):
            state.running.set()
            break

        parts = cmd.split()
        if not parts:
            continue

        match parts[0]:
            case "send":
                if len(parts) != 5:
                    print("Syntax: send <client_id> <func_code> <flags> <data>")
                    continue

                client_id = int(parts[1])
                func_input = parts[2]
                flags = int(parts[3])
                data = parts[4].encode("utf-8")
                
                func_val = 0
                
                if func_input.isdigit():
                    func_val = int(func_input)
                else:
                    try:
                        # .upper() pozwala wpisać "cat_sim_api" małymi literami
                        func_val = FuncCode[func_input.upper()].value
                    except KeyError:
                        valid_codes = [f.name for f in FuncCode]
                        print(f"Błąd: '{func_input}' nie jest liczbą ani znanym kodem.")
                        print(f"Dostępne kody: {valid_codes}")
                        continue
               
                
                async with state.clients_lock:
                    if client_id not in state.clients:
                        print("Unknown client id")
                        continue
                    client = state.clients[client_id]

                # Tutaj func_val jest już intem (niezależnie czy wpisano "1" czy "CAT_...")
                header = build_header(len(data), func_val, flags)
                
                try:
                    client.writer.write(header + data)
                    await client.writer.drain()
                    print(f"Sent func={func_val} to client {client_id}")
                except Exception as e:
                    print(f"Send error: {e}")

            case "list":
                async with state.clients_lock:
                    if not state.clients:
                        print("No devices connected")
                    else:
                        for i, c in state.clients.items():
                            print(f"{i}   {c.addr}   connected={c.connected}")

            case "exit":
                state.running.set()

            case "help":
                print("""
send    Send message to device.
        send <client_id> <func_code> <flags> <data>
list    List connected devices.
exit    Stop server.
""")

            case _:
                print("Unknown command. Try 'help'.")


async def handle_client(state: AppState, client_id: int, reader: asyncio.StreamReader, writer: asyncio.StreamWriter):
    addr = writer.get_extra_info("peername")

    async with state.clients_lock:
        state.clients[client_id] = ClientInfo(addr=addr, writer=writer)

    try:
        while not state.running.is_set():
            try:
                header = await reader.readexactly(4)
            except asyncio.IncompleteReadError:
                break
            length, func_code, flags = extract_header(header)
            
            try:
                data = await reader.readexactly(length)
            except asyncio.IncompleteReadError:
                # Jeśli klient rozłączy się w trakcie wysyłania danych
                break
                
            message = data.decode("utf-8")
            print(f"l = {length}, fc = {func_code}, fl = {flags}, d = {message}")
            async with state.clients_lock:
                state.clients[client_id].last_msg = message

            # response = f"ACK [{func_code}]".encode("utf-8")
            # writer.write(response)
            # await writer.drain()

    except Exception as e:
        print(f"Server Error for client {client_id}: {e}")  # <--- Dodaj printowanie błędu na konsolę
        async with state.clients_lock:
            if client_id in state.clients:
                state.clients[client_id].last_msg = f"ERROR: {e}"
    finally:
        async with state.clients_lock:
            if client_id in state.clients:
                state.clients[client_id].connected = False

        writer.close()
        try:
            await writer.wait_closed()
        except:
            pass


async def run_tcp_server(state: AppState):
    async def client_connected(reader, writer):
        async with state.clients_lock:
            state.client_id_counter += 1
            cid = state.client_id_counter

        asyncio.create_task(handle_client(state, cid, reader, writer))

    server = await asyncio.start_server(client_connected, HOST, PORT, limit=BUFFER_SIZE)

    async with server:
        await server.serve_forever()



async def main():
    state = AppState()

    server_task = asyncio.create_task(run_tcp_server(state))
    console_task = asyncio.create_task(console(state))

    await state.running.wait()

    server_task.cancel()
    console_task.cancel()

    await asyncio.gather(server_task, console_task, return_exceptions=True)


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        sys.exit()
