#include <zephyr/kernel.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "uart_serializer.h"

// Maksymalne ograniczenia
#define MAX_TOKENS 10
#define MAX_ARGS   5

// Typy argumentów
typedef enum { ARG_END = 0, ARG_INT, ARG_STR, ARG_IP } ArgType;

// Struktura wpisu w tabeli komend
typedef struct {
    const char* main_cmd;  // np. "neighbor"
    const char* sub_cmd;   // np. "ping"
    const char* keyword;   // opcjonalne (np. NULL)
    uint16_t    func_code; // np. (CAT_SYSTEM<<8)|SYS_PING
    ArgType     args[MAX_ARGS];
} CommandEntry;

// === TABELA KOMEND (Tu definiujesz logikę) ===
static const CommandEntry CMD_TABLE[] = {
    { "neighbor", "ping",       NULL, (CAT_SYSTEM<<8)|SYS_PING,     { ARG_IP, ARG_END } },
    { "neighbor", "connect",    NULL, (CAT_SYSTEM<<8)|SYS_CONN,     { ARG_IP, ARG_INT, ARG_END } }, // Poprawiono: dodano ARG_INT dla portu
    { "neighbor", "disconnect", NULL, (CAT_SYSTEM<<8)|SYS_DISCONN,  { ARG_END } },
    { "neighbor", "send",       NULL, (CAT_SYSTEM<<8)|SYS_RAW_SEND, { ARG_STR, ARG_END } },
    { NULL, NULL, NULL, 0, {0} } // Sentinel (koniec tabeli)
};

// --- Funkcje Pomocnicze (Lokalne) ---

static void pack_uint32(uint8_t* buf, uint32_t val) {
    buf[0] = (val >> 24) & 0xFF;
    buf[1] = (val >> 16) & 0xFF;
    buf[2] = (val >> 8)  & 0xFF;
    buf[3] = (val >> 0)  & 0xFF;
}

// Implementacja pack_header (format: [Len 4b][Func 12b][Flags 8b])
// Uwaga: Len w tym formacie to górne 4 bity bajtu 0 i dolne 4 bity bajtu 1.
static void pack_header(uint8_t* buf, uint16_t total_len, uint16_t func_code, uint8_t flags) {
    // Bajt 0: Len[11:4]
    buf[0] = (total_len >> 4) & 0xFF;
    // Bajt 1: Len[3:0] | Func[11:8]
    buf[1] = ((total_len & 0x0F) << 4) | ((func_code >> 8) & 0x0F);
    // Bajt 2: Func[7:0]
    buf[2] = func_code & 0xFF;
    // Bajt 3: Flags
    buf[3] = flags;
}

// Oryginalny tokenizer z obsługą cudzysłowów
int tokenize_command(char* cmd, char* tokens[], int max_tokens) {
    int count = 0;
    char* ptr = cmd;

    while (*ptr != '\0' && count < max_tokens) {
        // 1. Pomiń spacje
        while (isspace((unsigned char)*ptr)) ptr++;
        if (*ptr == '\0') break;

        // 2. Znajdź początek tokena
        char* start = ptr;
        char quote_char = 0;

        if (*ptr == '"' || *ptr == '\'') {
            quote_char = *ptr;
            start = ptr + 1; // Treść zaczyna się za cudzysłowem
            ptr++;
        }

        // 3. Znajdź koniec tokena
        while (*ptr != '\0') {
            if (quote_char) {
                if (*ptr == quote_char) { // Koniec cudzysłowu
                    *ptr = '\0';
                    ptr++;
                    break;
                }
            } else {
                if (isspace((unsigned char)*ptr)) { // Koniec słowa
                    *ptr = '\0';
                    ptr++;
                    break;
                }
            }
            ptr++;
        }
        tokens[count++] = start;
    }
    return count;
}

// --- Główna Funkcja Serializująca ---

uint8_t* serialize_command_alloc(char* input_cmd, size_t* out_len) {
    if (!input_cmd) return NULL;

    // Kopia robocza, bo tokenizer niszczy stringa
    char cmd_copy[128];
    strncpy(cmd_copy, input_cmd, sizeof(cmd_copy)-1);
    cmd_copy[sizeof(cmd_copy)-1] = '\0';

    char* tokens[MAX_TOKENS];
    int token_count = tokenize_command(cmd_copy, tokens, MAX_TOKENS);

    if (token_count < 2) return NULL;

    // 1. Znajdź pasującą komendę w tabeli
    const CommandEntry* match = NULL;
    int arg_start_idx = 0;

    for (int i = 0; CMD_TABLE[i].main_cmd != NULL; i++) {
        const CommandEntry* e = &CMD_TABLE[i];
        // Sprawdź main_cmd i sub_cmd
        if (strcmp(tokens[0], e->main_cmd) == 0 && strcmp(tokens[1], e->sub_cmd) == 0) {
            // Obsługa opcjonalnego słowa kluczowego (np. "neighbor connect TO ...")
            if (e->keyword) {
                if (token_count > 2 && strcmp(tokens[2], e->keyword) == 0) {
                    match = e; arg_start_idx = 3; break;
                }
            } else {
                match = e; arg_start_idx = 2; break;
            }
        }
    }

    if (!match) return NULL; // Nieznana komenda

    // 2. PASS 1: Oblicz rozmiar payloadu
    size_t payload_size = 0;
    int curr_tok = arg_start_idx;

    for (int i = 0; i < MAX_ARGS; i++) {
        if (match->args[i] == ARG_END) break;
        if (curr_tok >= token_count) return NULL; // Brakujące argumenty

        switch (match->args[i]) {
            case ARG_INT: 
                payload_size += 4; // uint32
                break;
            case ARG_STR: 
            case ARG_IP:
                payload_size += (strlen(tokens[curr_tok]) + 1); // string + null
                break;
            default: break;
        }
        curr_tok++;
    }

    size_t total_size = 4 + payload_size; // Header (4B) + Payload

    // 3. Alokacja (Zephyr Heap)
    uint8_t* packet = (uint8_t*)k_malloc(total_size);
    if (!packet) return NULL;

    // 4. PASS 2: Zapisz dane
    pack_header(packet, (uint16_t)total_size, match->func_code, 0x00);
    
    size_t offset = 4;
    curr_tok = arg_start_idx;

    for (int i = 0; i < MAX_ARGS; i++) {
        if (match->args[i] == ARG_END) break;

        char* val_str = tokens[curr_tok++];

        switch (match->args[i]) {
            case ARG_INT: {
                uint32_t val = (uint32_t)strtol(val_str, NULL, 10);
                pack_uint32(&packet[offset], val);
                offset += 4;
                break;
            }
            case ARG_STR: 
            case ARG_IP: {
                size_t len = strlen(val_str) + 1;
                memcpy(&packet[offset], val_str, len);
                offset += len;
                break;
            }
            default: break;
        }
    }

    if(out_len) *out_len = total_size;
    return packet;
}

void unpack_header(const uint8_t* buf, uint16_t* out_len, uint16_t* out_func) {
    if (!buf || !out_len || !out_func) return;

    // Format (wg Twojego kodu):
    // Byte 0: Len[11:4]
    // Byte 1: Len[3:0] | Func[11:8]
    // Byte 2: Func[7:0]
    // Byte 3: Flags

    // 1. Odczyt długości (12 bitów)
    *out_len = (uint16_t)((buf[0] << 4) | (buf[1] >> 4));

    // 2. Odczyt kodu funkcji (12 bitów)
    *out_func = (uint16_t)(((buf[1] & 0x0F) << 8) | buf[2]);
}