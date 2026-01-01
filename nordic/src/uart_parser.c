#include <stdint.h>       // Naprawia błąd: uint8_t undefined

#include "command.h"      // Naprawia błąd: Command undefined
#include "uart_parser.h"  // Dobra praktyka

void uart_parser_feed(uint8_t byte, osMessageQueueId_t* queue) {
    static uint8_t internal_buffer[64];
    static int idx = 0;

    // Prosta logika: zbieraj do '\n' i wtedy parsuj
    if (byte == '\n' || byte == '\r') {
        if (idx > 0) {
            internal_buffer[idx] = 0; // Null-terminate
            
            // Tutaj wywołujesz swoją oryginalną logikę parsowania
            Command cmd;
            if (parse_line((char*)internal_buffer, &cmd)) { 
                osMessageQueuePut(*queue, &cmd, 0, 0);
            }
            idx = 0;
        }
    } else {
        if (idx < 63) internal_buffer[idx++] = byte;
    }
}