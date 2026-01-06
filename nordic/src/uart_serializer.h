#ifndef UART_SERIALIZER_H
#define UART_SERIALIZER_H

#include <stdint.h>
#include <stddef.h>

// === Definicje Kodów (Zgodne z LPC) ===
#define CAT_SYSTEM      0x0F
#define SYS_PING        0x01
#define SYS_CONN        0x02
#define SYS_DISCONN     0x03
#define SYS_RAW_SEND    0x04

// === Funkcje ===

/**
 * @brief Parsuje komendę tekstową i zamienia ją od razu na pakiet binarny.
 * * @param input_cmd Ciąg znaków np. "neighbor ping 1.2.3.4"
 * @param out_len   Wskaźnik, gdzie zostanie zapisana długość wygenerowanego pakietu
 * @return uint8_t* Wskaźnik do zaalokowanego bufora z pakietem (należy zwolnić k_free)
 * lub NULL w przypadku błędu.
 */
uint8_t* serialize_command_alloc(char* input_cmd, size_t* out_len);

void unpack_header(const uint8_t* buf, uint16_t* out_len, uint16_t* out_func);

#endif // UART_SERIALIZER_H