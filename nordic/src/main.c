#include <zephyr/kernel.h>
#include <dk_buttons_and_leds.h> // Biblioteka do obsługi LED na płytce DK
#include <cmsis_os2.h>

// Twoje nagłówki
#include "command.h"
#include "slab_alloc.h"
#include "if_dispatcher.h"
#include "uart_handle.h"

// Definicje diod (z biblioteki DK)
#define LED_HEARTBEAT  DK_LED1
#define LED_ERROR      DK_LED2

// Kolejki systemowe
osMessageQueueId_t in_q = NULL;
osMessageQueueId_t out_q = NULL;

// Hook błędu RTOS
void vApplicationStackOverflowHook(void *xTask, char *pcTaskName) {
    while(1) {
        dk_set_led(LED_ERROR, 1);
        k_msleep(100);
        dk_set_led(LED_ERROR, 0);
        k_msleep(100);
    }
}
__NO_RETURN void app_main(void *argument) {
    uint32_t tick = 0;
    
    // Inicjalizacja UART
    if (initialize_uart_int(in_q, out_q) != 0) {
        dk_set_led(LED_ERROR, 1);
    } else {
        uart_printn("SYSTEM BOOT: nRF52840 Ready\r\n");
        uart_printn("Wpisz cos i wcisnij Enter...\r\n"); // Instrukcja dla użytkownika
    }

    Command cmd_rx;

    for(;;) {
        // --- LOGIKA ECHO ---
        // Sprawdzamy, czy w kolejce wejściowej (in_q) jest jakaś komenda
        // Czekamy 0ms (nie blokujemy pętli)
        if (osMessageQueueGet(in_q, &cmd_rx, NULL, 0) == osOK) {
            
            // Mamy komendę! Normalnie tu byś ją analizował.
            // Ale my chcemy tylko sprawdzić czy działa, więc odsyłamy ją z powrotem.
            
            // 1. Wyświetl informację, że coś odebrano
            uart_printn("Odebrano: ");
            
            // 2. Wyślij treść komendy z powrotem na terminal
            if (cmd_rx.data_ptr != NULL) {
                uart_printn((char*)cmd_rx.data_ptr);
                uart_printn("\r\n");
                
                // UWAGA: Ponieważ tylko wyświetlamy, a nie przekazujemy dalej do out_q,
                // musimy tutaj zwolnić pamięć, żeby nie było wycieku!
                slab_free(cmd_rx.data_ptr);
            }
        }
        // -------------------

        if (tick % 100 == 0) {
            static int led_state = 0;
            led_state = !led_state;
            dk_set_led(LED_HEARTBEAT, led_state);
        }
        
        tick_dispatcher(out_q); // To obsługuje kolejkę wyjściową (gdybyś używał out_q)
        osDelay(10); 
        tick++;
    }
}

int main(void) {
    // 1. Inicjalizacja diod
    if (dk_leds_init() != 0) {
        return -1;
    }

    // 2. Inicjalizacja pamięci (Twoja biblioteka)
    slab_init();

    // 3. Kolejki
    in_q = osMessageQueueNew(10, sizeof(Command), NULL);
    out_q = osMessageQueueNew(10, sizeof(Command), NULL);
    
    // 4. Wątek główny aplikacji
    osThreadAttr_t main_attr = {
        .name = "main_thread",
        .priority = osPriorityNormal,
        .stack_size = 4096,
    };

    // W Zephyrze kernel już działa. Tworzymy wątek i on ruszy natychmiast.
    osThreadNew(app_main, NULL, &main_attr);

    // UWAGA: Nie wywołujemy osKernelStart(), bo Zephyr już działa!
    // Funkcja main w Zephyrze jest traktowana jak zwykły wątek.
    // Możemy tu po prostu zasnąć na zawsze.
    for(;;) {
        k_sleep(K_FOREVER);
    }
}