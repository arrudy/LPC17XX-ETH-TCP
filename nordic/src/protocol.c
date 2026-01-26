#include <zephyr/kernel.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "protocol.h"


static int parse_ip_str(const char *ip_str, uint8_t *out_ip) {
    int values[4];
    if (sscanf(ip_str, "%d.%d.%d.%d", &values[0], &values[1], &values[2], &values[3]) != 4) return -1;
    for(int i=0; i<4; i++) out_ip[i] = (uint8_t)values[i];
    return 0;
}
static void pack_header(uint8_t* buf, uint16_t total_len, uint16_t func_code) {
    buf[0] = (total_len >> 4) & 0xFF;
    buf[1] = ((total_len & 0x0F) << 4) | ((func_code >> 8) & 0x0F);
    buf[2] = func_code & 0xFF;
    buf[3] = 0x00;
}

int parse_text_command(char *text, ProtocolMsg *out_msg) {
    char *saveptr;
    char cmd_buf[128]; 
    strncpy(cmd_buf, text, sizeof(cmd_buf)-1);
    cmd_buf[sizeof(cmd_buf)-1] = '\0';

    char *token = strtok_r(cmd_buf, " \r\n", &saveptr);
    if (!token) return -1;

    // Domyślnie odpowiedź trafia na CLI, chyba że zmienimy na UART3
    out_msg->target_if = IF_CLI; 

    if (strcmp(token, "neighbor") == 0) {
        char *subcmd = strtok_r(NULL, " \r\n", &saveptr);
        if (!subcmd) return -1;

        if (strcmp(subcmd, "ping") == 0) {
             // Ping akurat w Twoim starym kodzie szedł na UART, 
             // ale możemy go wysłać na UART3 jeśli to test łącza.
             // Zostawmy na razie na CLI (jako echo) lub UART3 wedle uznania.
             // W LPC ping był "lokalny".
             char *ip = strtok_r(NULL, " \r\n", &saveptr);
             if (!ip) return -1;
             out_msg->func_code = (CAT_SYSTEM << 8) | SYS_PING;
             out_msg->payload_len = strlen(ip) + 1;
             out_msg->payload = k_malloc(out_msg->payload_len);
             strcpy(out_msg->payload, ip);
             return 0;
        }
        else if (strcmp(subcmd, "connect") == 0) {
            // CONNECT -> Logika połączenia idzie na UART3
            char *ip = strtok_r(NULL, " \r\n", &saveptr);
            char *port_str = strtok_r(NULL, " \r\n", &saveptr);
            if (!ip || !port_str) return -1;

            uint8_t ip_bytes[4];
            if (parse_ip_str(ip, ip_bytes) != 0) return -2;
            int port = atoi(port_str);

            out_msg->func_code = (CAT_SYSTEM << 8) | SYS_CONN;
            out_msg->payload_len = 6;
            out_msg->payload = k_malloc(6);
            memcpy(out_msg->payload, ip_bytes, 4);
            out_msg->payload[4] = (port >> 8) & 0xFF;
            out_msg->payload[5] = port & 0xFF;
            
            out_msg->target_if = IF_UART3; // Kierujemy do UART3
            return 0;
        }
        else if (strcmp(subcmd, "disconnect") == 0) {
            out_msg->func_code = (CAT_SYSTEM << 8) | SYS_DISCONN;
            out_msg->payload_len = 0;
            out_msg->payload = NULL;
            out_msg->target_if = IF_UART3; // Kierujemy do UART3
            return 0;
        }
        else if (strcmp(subcmd, "send") == 0) {
            // SEND -> Dane lecą na UART3
            char *msg = strtok_r(NULL, "\r\n", &saveptr);
            if (!msg) return -1;

            out_msg->func_code = (CAT_SYSTEM << 8) | SYS_RAW_SEND;
            out_msg->payload_len = strlen(msg) + 1;
            out_msg->payload = k_malloc(out_msg->payload_len);
            strcpy(out_msg->payload, msg);
            
            out_msg->target_if = IF_UART3; // Kierujemy do UART3
            return 0;
        }
    }
    return -1;
}

int serialize_message(ProtocolMsg *msg, uint8_t *buffer, size_t max_len) {
    size_t total_len = 4 + msg->payload_len;
    if (total_len > max_len) return -2;
    pack_header(buffer, (uint16_t)total_len, msg->func_code);
    if (msg->payload && msg->payload_len > 0) {
        memcpy(buffer + 4, msg->payload, msg->payload_len);
    }
    return total_len;
}

void free_protocol_msg(ProtocolMsg *msg) {
    if (msg->payload) {
        k_free(msg->payload);
        msg->payload = NULL;
    }
}