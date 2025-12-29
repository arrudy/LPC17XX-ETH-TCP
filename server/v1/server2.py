import asyncio
import socket
from dataclasses import dataclass, field
from rich.table import Table
from rich.live import Live
from prompt_toolkit import PromptSession
from typing import Dict
import sys

HOST = "127.0.0.1"
PORT = 65432
MAX_CONNECTIONS = 5
BUFFER_SIZE = 2048

FUNC_CODES = {"CAT_SIM_API", "CAT_SIM_NOTIF", "CAT_SYS_NOTIF"}


# =========================
# STATE
# =========================

@dataclass
class ClientInfo:
    addr: tuple
    writer: asyncio.StreamWriter
    last_msg: str = ""
    connected: bool = True


@dataclass
class AppState:
    clients: Dict[int, ClientInfo] = field(default_factory=dict)
    running: bool = True
    client_id_counter: int = 0


# =========================
# PROTOCOL
# =========================

def extract_header(header: bytes):
    header_int = int.from_bytes(header, byteorder="big")
    mask_12_bit = (1 << 12) - 1
    mask_8_bit = (1 << 8) - 1

    length = (header_int >> 20) & mask_12_bit
    function_code = (header_int >> 8) & mask_12_bit
    flags = header_int & mask_8_bit

    return length, function_code, flags


# =========================
# TCP CLIENT HANDLER
# =========================

async def handle_client(
    state: AppState,
    client_id: int,
    reader: asyncio.StreamReader,
    writer: asyncio.StreamWriter,
):
    addr = writer.get_extra_info("peername")
    state.clients[client_id] = ClientInfo(addr=addr, writer=writer)

    try:
        while state.running:
            header = await reader.readexactly(4)
            length, func_code, flags = extract_header(header)

            data = await reader.readexactly(length)
            message = data.decode("utf-8")

            state.clients[client_id].last_msg = message

            response = f"ACK [{func_code}]".encode("utf-8")
            writer.write(response)
            await writer.drain()

    except asyncio.IncompleteReadError:
        pass
    except Exception as e:
        state.clients[client_id].last_msg = f"ERROR: {e}"
    finally:
        state.clients[client_id].connected = False
        writer.close()
        await writer.wait_closed()


# =========================
# TCP SERVER
# =========================

async def run_tcp_server(state: AppState):
    async def client_connected(reader, writer):
        state.client_id_counter += 1
        cid = state.client_id_counter
        asyncio.create_task(handle_client(state, cid, reader, writer))

    server = await asyncio.start_server(
        client_connected, HOST, PORT, limit=BUFFER_SIZE
    )

    async with server:
        await server.serve_forever()


# =========================
# CONSOLE INPUT
# =========================

async def console_input(state: AppState):
    session = PromptSession()

    while state.running:
        try:
            cmd = await session.prompt_async("> ")
        except (EOFError, KeyboardInterrupt):
            state.running = False
            break

        parts = cmd.split()
        if not parts:
            continue

        match parts[0]:
            case "exit":
                state.running = False

            case "list":
                pass  # UI shows this

            case "send":
                if len(parts) != 5:
                    continue

                cid = int(parts[1])
                payload = parts[4]

                if cid in state.clients:
                    writer = state.clients[cid].writer
                    writer.write(payload.encode("utf-8"))
                    await writer.drain()

            case "help":
                pass


# =========================
# RICH UI
# =========================

def render(state: AppState) -> Table:
    table = Table(title="Connected Clients")
    table.add_column("ID")
    table.add_column("Address")
    table.add_column("Connected")
    table.add_column("Last Message")

    for cid, c in state.clients.items():
        table.add_row(
            str(cid),
            str(c.addr),
            "YES" if c.connected else "NO",
            c.last_msg,
        )

    return table


async def run_ui(state: AppState):
    with Live(render(state), refresh_per_second=5):
        while state.running:
            await asyncio.sleep(0.2)


# =========================
# MAIN
# =========================

async def main():
    state = AppState()

    await asyncio.gather(
        run_tcp_server(state),
        console_input(state),
        run_ui(state),
    )


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        sys.exit()
