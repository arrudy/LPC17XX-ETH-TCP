import socket
import threading
from usecases import func_runner
import sys

HOST = '127.0.0.1'  # Standardowy adres loopback (localhost)
PORT = 65432        # Port do nasłuchiwania (nieużywany port > 1023)
BUFFER_SIZE = 2048  # Wielkość bufora do odbierania danych
MAX_CONNECTIONS = 5

clients = {}
clients_lock = threading.Lock()
client_id_counter = 0


func_codes = {"CAT_SIM_API", "CAT_SIM_NOTIF", "CAT_SYS_NOTIF"}


def console():
    global clients
    def commands_parser(from_console):
        match from_console[0]:
            case "send":
                if len(from_console) == 5:
                    with clients_lock:
                        ids =[ id[1] for id in clients.values()]
                        if from_console[1] in ids:
                            if from_console[2] in func_codes:
                                pass
                            else:
                                print("Unknown func code")
                        else:
                            print("Unknown id")
                        
                else:
                    print("Invalid syntax")
                    
            case "list":
                for i, c in clients.items():
                    
                    print(f"{i}   {c[-1]}")
            case "exit":
                sys.exit(0)
            
            case "help":
                help_message = """
                send    Send message to available devices.
                    send <device_id> <func_code> <flags> <data>
                list    List availble devices.
                """
                print(help_message)
            case _:
                print("Invalid command try help")
                
    
    while True:
        from_console = input("> ")
        from_console = from_console.split()
        if len(from_console)>0:
            commands_parser(from_console)
        
        
        
def extract_header(header):
    header_int = int.from_bytes(header, byteorder='big')
    mask_12_bit = (1 << 12) - 1 
    mask_8_bit = (1 << 8) - 1

    length = (header_int >> 20) & mask_12_bit
    function_code = (header_int >> 8) & mask_12_bit
    flags = header_int & mask_8_bit
    
    return length, function_code, flags
                
def handle_client(client_id, conn, addr):
    global clients
    global clients_lock
    global client_id_counter
    print(f"[AKTYWNE] {addr} | {client_id}")
    
    with conn:
        try:
            while True:
                header = conn.recv(4)
                length, func_code, flags = extract_header(header)
                print(f"[{client_id}] L= {length}, FC= {func_code}, F= {flags}")
                data = conn.recv(int(length))
                data = data.decode('utf-8')
                
                if not data:
                    break
                print(data)
                #message = func_runner(func_code, flags, data)
                message = "Odpowiedz"
                
                response = message.encode('utf-8')
                conn.sendall(response)
                
                
        except ConnectionResetError:
            # Obsługa sytuacji, gdy klient nagle zamknie połączenie
            print(f"[{addr} | {client_id}] Połączenie przerwane przez klienta.")
        except Exception as e:
            print(f"[{addr} | {client_id}] Wystąpił błąd: {e}")
        finally:
            with clients_lock:
                clients.pop(client_id, None)
    print(f"[ZAKOŃCZONO] {addr} | {client_id} zamknięte.")
                

def run_tcp_server():
    global client_id_counter
    
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        
        s.bind((HOST, PORT))
        s.listen(MAX_CONNECTIONS)
        print(f"*** Serwer nasłuchuje na {HOST}:{PORT} ***")

        while True:
            conn, addr = s.accept()
            with clients_lock:
                client_id_counter += 1
                client_id = client_id_counter
                clients[client_id] = (conn, addr)

                t = threading.Thread(
                    target=handle_client,
                    args=(client_id, conn, addr),
                    daemon=True
                )
                t.start()
                print(f"[INFO] Klient #{client_id} {addr} podłączony")
                


if __name__ == "__main__":
    server_thread = threading.Thread(target=run_tcp_server)
    server_thread.start()
    console()