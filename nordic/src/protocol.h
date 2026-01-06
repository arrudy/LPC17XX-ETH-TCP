#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

// Kody funkcji (bez zmian)
#define CAT_SYSTEM      0x0F
#define SYS_PING        0x01
#define SYS_CONN        0x02
#define SYS_DISCONN     0x03
#define SYS_RAW_SEND    0x04

// Definicja interfejsów
typedef enum {
    IF_CLI   = 0, // Konsola sterująca
    IF_UART3 = 1, // Kanał danych (funkcjonalny odpowiednik ETH)
} InterfaceType;

typedef struct {
    uint16_t func_code;
    InterfaceType target_if; 
    uint8_t *payload;
    size_t payload_len;
} ProtocolMsg;

int parse_text_command(char *text, ProtocolMsg *out_msg);
int serialize_message(ProtocolMsg *msg, uint8_t *buffer, size_t max_len);
void free_protocol_msg(ProtocolMsg *msg);

#endif