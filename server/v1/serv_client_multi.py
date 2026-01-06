import socket
import threading
import time
import random

# Konfiguracja serwera
HOST = '127.0.0.1'
PORT = 5000
BUFFER_SIZE = 1024
NUM_CONNECTIONS = 10 # Liczba klientów, którą chcemy symulować

# Funkcja obsługująca pojedyncze połączenie klienta
def connect_and_test(client_id):
    """
    Nawiązuje, testuje i zamyka pojedyncze połączenie z serwerem.
    """
    try:
        # 1. Tworzenie gniazda i nawiązywanie połączenia
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            # s.settimeout(5.0) 
            print(f"[Klient {client_id}] Próba połączenia...")
            s.connect((HOST, PORT))
            print(f"[Klient {client_id}] Połączono pomyślnie.")

            # 2. Wysyłanie danych
            message = f"5du"
            s.sendall(message.encode('utf-8'))
            print(f"[Klient {client_id}] Wysłano wiadomość.")

            # 3. Odbieranie odpowiedzi
            data = s.recv(BUFFER_SIZE)
            if data:
                print(f"[Klient {client_id}] Odebrano odpowiedź: {data.decode('utf-8')}")
            else:
                print(f"[Klient {client_id}] Błąd: Serwer nie wysłał odpowiedzi lub zamknął połączenie.")

            # 4. Utrzymanie połączenia aktywnego (symulacja pracy)
            delay = random.uniform(0.5, 5.0) # Losowy czas od 0.5s do 2s
            print(f"[Klient {client_id}] Utrzymywanie połączenia przez {delay:.2f}s...")
            time.sleep(delay)
            
        # Po wyjściu z bloku 'with' połączenie jest automatycznie zamykane
        print(f"[Klient {client_id}] Połączenie zamknięte.")

    except ConnectionRefusedError:
        print(f"[Klient {client_id}] BŁĄD: Nie udało się połączyć. Upewnij się, że serwer działa.")
    except Exception as e:
        print(f"[Klient {client_id}] Wystąpił nieoczekiwany błąd: {e}")


def run_test_client():
    threads = []
    
    print(f"*** Start symulacji {NUM_CONNECTIONS} jednoczesnych klientów ***")
    
    for i in range(1, NUM_CONNECTIONS + 1):
        # Tworzenie i uruchamianie wątku dla każdego symulowanego klienta
        thread = threading.Thread(target=connect_and_test, args=(i,))
        threads.append(thread)
        thread.start()
        # Małe opóźnienie, aby klienci nie startowali w idealnie tej samej mikrosekundzie
        time.sleep(0.05) 

    # Czekanie na zakończenie wszystkich wątków (klientów)
    for thread in threads:
        thread.join()
        
    print("*** Wszystkie testy klientów zakończone. ***")


if __name__ == "__main__":
    run_test_client()