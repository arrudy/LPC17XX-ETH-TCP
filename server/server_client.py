import socket

HOST = '127.0.0.1'  # Adres serwera
PORT = 65432        # Port serwera

def run_tcp_client():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        try:
            # Nawiązanie połączenia z serwerem
            s.connect((HOST, PORT))
            print(f"Połączono z serwerem {HOST}:{PORT}")

            # Wysyłanie wiadomości
            message = "Witaj serwerze TCP!"
            s.sendall(message.encode('utf-8'))
            print(f"Wysłano: {message}")

            # Odbieranie odpowiedzi
            data = s.recv(1024)
            print(f"Odebrano od serwera: {data.decode('utf-8')}")

        except ConnectionRefusedError:
            print("Nie udało się połączyć: Serwer nie jest uruchomiony.")
        except Exception as e:
            print(f"Wystąpił błąd: {e}")

if __name__ == "__main__":
    run_tcp_client()