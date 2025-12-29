import asyncio
import sys
from transport import TransportManager
from app import MyApplication

CONFIG = [
    # Typ Transportu   Podtyp (Logic)    Adres/Port      Baud    Etykieta
    ("TCP",            None,             "0.0.0.0",      5000,   "ETHERNET"),
    ("SERIAL",         "GATEWAY",        "/dev/ttyUSB0", 115200, "RADIO_802.15.4"),
    ("SERIAL",         "DIRECT",         "/dev/ttyS0",   9600,   "UART_WIRED"),
]

async def main():
    print("--- START SYSTEMU ---")
    
    # 1. Tworzenie Managera
    tm = TransportManager()
    
    # 2. Tworzenie Aplikacji
    app = MyApplication(tm)

    active_servers = []

    # 3. Uruchamianie interfejsów
    for item in CONFIG:
        trans_type, sub_type, addr, port_baud, label = item
        
        try:
            if trans_type == "TCP":
                # TCP zawsze zwraca obiekt serwera, który trzeba potem zamknąć
                srv = await tm.start_tcp(addr, port_baud)
                active_servers.append(srv)
                
            elif trans_type == "SERIAL":
                if sub_type == "GATEWAY":
                    # Radio (wiele urządzeń na jednym porcie)
                    await tm.start_radio_gateway(addr, port_baud, label=label)
                else:
                    # Zwykły kabel (jedno urządzenie)
                    await tm.start_direct_uart(addr, port_baud, label=label)
                
        except (FileNotFoundError, OSError) as e:
            print(f"⚠️  [OSTRZEŻENIE] Nie udało się uruchomić {label} ({addr}): {e}")
        except Exception as e:
            print(f"❌ [BŁĄD KRYTYCZNY] {label}: {e}")

    print("\nSystem gotowy. Uruchamiam konsolę...")
    
    try:
        await app.run_console()
    finally:
        print("\n--- ZAMYKANIE ---")
        
        # KROK 1: Zamykanie serwerów TCP z timeoutem
        print("1. Zamykanie nasłuchiwania TCP...")
        if active_servers:
            
            for srv in active_servers:
                srv.close()
            
            try:
                wait_tasks = [srv.wait_closed() for srv in active_servers]
                await asyncio.wait_for(asyncio.gather(*wait_tasks), timeout=2.0)
            except asyncio.TimeoutError:
                print("⚠️ [Main] Timeout: Serwery TCP zamykane siłowo.")
            except Exception as e:
                print(f"⚠️ [Main] Błąd przy zamykaniu serwerów: {e}")
        
        print("2. Wyłączanie TransportManagera...")
        
        try:
            await asyncio.wait_for(tm.shutdown(), timeout=5.0)
        except asyncio.TimeoutError:
                print("⚠️ [Main] Timeout: Manager transportu zamykany siłowo.")

        print("Do widzenia.")

if __name__ == "__main__":
    try:
        if sys.platform == 'win32':
            asyncio.set_event_loop_policy(asyncio.WindowsSelectorEventLoopPolicy())
        asyncio.run(main())
    except KeyboardInterrupt:
        pass