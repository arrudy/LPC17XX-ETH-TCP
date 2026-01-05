#include "uart_serializer.h"
#include "command.h"
#include "string.h"
#include <ctype.h>


typedef enum { ARG_END = 0, ARG_INT, ARG_STR, ARG_IP } ArgType;

typedef struct {
    const char* main_cmd;
    const char* sub_cmd;
    const char* keyword;
    uint16_t    func_code;
    ArgType     args[MAX_ARGS];
} CommandEntry;



static const CommandEntry CMD_TABLE[] = {
    { "neighbor", "ping", NULL,      (CAT_SYSTEM<<8)|SYS_PING,     { ARG_IP, ARG_END } },
    { "neighbor", "connect", NULL,   (CAT_SYSTEM<<8)|SYS_CONN,     { ARG_IP, ARG_END } },
    { "neighbor", "disconnect", NULL,(CAT_SYSTEM<<8)|SYS_DISCONN,  { ARG_END } },
    { "neighbor", "send", NULL,      (CAT_SYSTEM<<8)|SYS_RAW_SEND, { ARG_STR, ARG_END } },
    //{ "self", "state", NULL,         (CAT_SIM<<8)|0x01,            { ARG_END } },
    //{ "self", "sleep", NULL,         (CAT_SIM<<8)|0x02,            { ARG_INT, ARG_END } },
    //{ "food", "list", NULL,          (CAT_SIM<<8)|0x03,            { ARG_END } },
    //{ "food", "eat",  NULL,          (CAT_SIM<<8)|0x04,            { ARG_INT, ARG_END } },
    //{ "food", "ask",  "neighbor",    (CAT_SIM<<8)|0x05,            { ARG_INT, ARG_STR, ARG_END } },
    { NULL, NULL, NULL, 0, {0} }
};

// --- Helper Functions ---




static void pack_uint32(uint8_t* buf, uint32_t val) {
    buf[0] = (val >> 24) & 0xFF;
    buf[1] = (val >> 16) & 0xFF;
    buf[2] = (val >> 8)  & 0xFF;
    buf[3] = (val >> 0)  & 0xFF;
}






int tokenize_command(char* cmd, char* tokens[], int max_tokens) {
    int count = 0;
    char* ptr = cmd;

    while (*ptr != '\0' && count < max_tokens) {
        // 1. Skip leading spaces
        while (isspace((unsigned char)*ptr)) {
            ptr++;
        }
        
        if (*ptr == '\0') break; // End of string found

        // 2. Identify start of token
        char* start = ptr;
        char quote_char = 0;

        if (*ptr == '"' || *ptr == '\'') {
            quote_char = *ptr;
            start = ptr + 1; // Start the token actual content AFTER the quote
            ptr++;           // Move inside the quote
        }

        // 3. Scan for end of token
        int closed = 0;
        while (*ptr != '\0') {
            if (quote_char) {
                // We are inside a quote, look for matching closing quote
                if (*ptr == quote_char) {
                    *ptr = '\0'; // Terminate string here
                    ptr++;       // Move past the closing quote
                    closed = 1;
                    break;
                }
            } else {
                // Standard word, look for space
                if (isspace((unsigned char)*ptr)) {
                    *ptr = '\0'; // Terminate string here
                    ptr++;       // Move past the space
                    break;
                }
            }
            ptr++;
        }
        
        // 4. Store the token
        tokens[count++] = start;
    }

    return count;
}






uint8_t* serialize_command_alloc(char* input_cmd, size_t* out_len) {
    if (!input_cmd ) return NULL;

    // 1. Tokenization (Stack copy to be safe)
//    char cmd_copy[256]; 
//    strncpy(cmd_copy, input_cmd, sizeof(cmd_copy)-1);
//    cmd_copy[sizeof(cmd_copy)-1] = '\0';

    char* tokens[MAX_TOKENS];
    int token_count = tokenize_command(input_cmd, tokens, MAX_TOKENS); //0;
    char* ctx;
    //char* t = strtok_r(input_cmd, " ", &ctx);
    /*while(t && token_count < MAX_TOKENS) {
        tokens[token_count++] = t;
        t = strtok_r(NULL, " ", &ctx);
    }*/

    if (token_count < 2) goto return_unknown;

    // 2. Find Match
    const CommandEntry* match = NULL;
    int arg_start_idx = 0;

    for (int i = 0; CMD_TABLE[i].main_cmd != NULL; i++) {
        const CommandEntry* e = &CMD_TABLE[i];
        if (strcmp(tokens[0], e->main_cmd) == 0 && strcmp(tokens[1], e->sub_cmd) == 0) {
            if (e->keyword) {
                if (token_count > 2 && strcmp(tokens[2], e->keyword) == 0) {
                    match = e; arg_start_idx = 3; break;
                }
            } else {
                match = e; arg_start_idx = 2; break;
            }
        }
    }

    if (!match) goto return_unknown;

    // 3. PASS 1: Calculate Required Size
    size_t payload_size = 0;
    int curr_tok = arg_start_idx;

    for (int i = 0; i < MAX_ARGS; i++) {
        if (match->args[i] == ARG_END) break;
        if (curr_tok >= token_count) goto return_unknown; // Missing args

        switch (match->args[i]) {
            case ARG_INT: 
                payload_size += 4; 
                break;
            case ARG_STR: 
            case ARG_IP:
                payload_size += (strlen(tokens[curr_tok]) + 1); 
                break;
            default: break;
        }
        curr_tok++;
    }

    size_t total_size = 4 + payload_size; // Header + Payload

    // 4. Allocate Memory (SLAB MALLOC)
    uint8_t* packet = (uint8_t*)slab_malloc(total_size);
    
    if (packet == NULL) {
        // Critical Error: Out of Memory
        return NULL;
    }

    // 5. PASS 2: Write Data
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

    if(out_len)
      *out_len = total_size;
    return packet;

return_unknown:
    // Fallback: Allocate small packet for error
    {
        size_t err_size = 4;
        uint8_t* err_pkt = (uint8_t*)slab_malloc(err_size);
        if (!err_pkt) return NULL; // Can't even allocate error packet!

        // Function 0xF01: System Notification - Unknown Command
        pack_header(err_pkt, err_size, (CAT_SYS_NOTIF << 8) | 0x01, 0x00);
        if(out_len)
          *out_len = err_size;
        return err_pkt;
    }
}
