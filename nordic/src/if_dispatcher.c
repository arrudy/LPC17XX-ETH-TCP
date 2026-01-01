
#include "if_dispatcher.h"

// --- Zmienna globalna (Naprawia błąd: out_queue undeclared) ---
static osMessageQueueId_t out_queue = NULL; 

// --- Worker ---
__NO_RETURN static void dispatcher_worker(void *argument)
{
  for(;;)
  {
    Command cmd;
    // Pobieramy komendę z kolejki
    osStatus_t status = osMessageQueueGet(out_queue, &cmd, NULL, osWaitForever);

    if (status == osOK) {
        if(cmd.interface & IF_UART)
        {
          uint8_t buffer[128];
          
          // Serializacja komendy do bufora
          int len = uart_serialize(&cmd, buffer, sizeof(buffer));

          if(len > 0) {
              if (len < 128) buffer[len] = 0; // Null-terminator dla bezpieczeństwa
              
              osStatus_t result = uart_printn((char*)buffer);
              
              if(result != osOK) {
                   // Opcjonalnie obsługa błędu
              }
          } else {
               uart_printn("!ERR serialize fail\r\n");
          }
        }
        
        // Zwalniamy pamięć alokowaną dla danych komendy
        if (cmd.data_ptr) {
            slab_free(cmd.data_ptr);
        }
    }
  }
}

// --- Funkcje Publiczne ---

osStatus_t init_if_dispatcher(osMessageQueueId_t out_q)
{
    out_queue = out_q; // Przypisanie do zmiennej globalnej

    osThreadAttr_t worker_attr = {
        .name = "if_dispatch",
        .priority = osPriorityNormal,
        .stack_size = 1024
    };

    // Użycie funkcji dispatcher_worker (Naprawia błąd: defined but not used)
    if (osThreadNew(dispatcher_worker, NULL, &worker_attr) == NULL) {
        return osError;
    }

    return osOK;
}

osStatus_t tick_dispatcher(osMessageQueueId_t out_q)
{
    // Funkcja cykliczna (jeśli potrzebna w main loop)
    return osOK;
}