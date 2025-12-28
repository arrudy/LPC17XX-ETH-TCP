#include "uart_parser.h"
#include "command.h"



// --- Helper: Big Endian Unpack ---
static uint32_t unpack_uint32(const uint8_t* buf) {
    return (uint32_t)((buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3]);
}

/**
 * @brief Appends src to dest safely, respecting max_len of dest.
 */
static void safe_cat(char* dest, const char* src, size_t dest_max_len) {
    size_t cur_len = strlen(dest);
    if (cur_len >= dest_max_len - 1) return; // No space

    size_t available = dest_max_len - cur_len - 1;
    strncat(dest, src, available);
}

/**
 * @brief Appends raw bytes (payload) as string safely.
 */
static void safe_cat_payload(char* dest, const uint8_t* payload, size_t p_len, size_t dest_max_len) {
    size_t cur_len = strlen(dest);
    if (cur_len >= dest_max_len - 1) return;

    size_t available = dest_max_len - cur_len - 1;
    size_t to_copy = (p_len < available) ? p_len : available;

    // We can use strncat because it stops at n OR null-terminator.
    // Casting raw bytes to char* is safe here for printing.
    strncat(dest, (const char*)payload, to_copy);
}







int parse_command(const uint8_t * p, char* buffer, size_t max_len) {
    if (!p || !buffer || max_len == 0) return -1;

    // 1. Initialize Buffer
    buffer[0] = '\0';


    // 2. Decode Header
    // Byte 0: Len[11:4]
    // Byte 1: Len[3:0] | Func[11:8]
    // Byte 2: Func[7:0]
    uint16_t length    = 0;//(uint16_t)((p[0] << 4) | ((p[1] >> 4) & 0x0F));
    uint16_t func_code = 0;//(uint16_t)(((p[1] & 0x0F) << 8) | p[2]);
  
    unpack_header(p, &length, &func_code, NULL);
    uint8_t  category  = (uint8_t)((func_code >> 8) & 0x0F);
    uint8_t  sub_cmd   = (uint8_t)(func_code & 0xFF);
  

  

    const uint8_t* payload = &p[4];
    size_t payload_len = (length > 4) ? (length - 4) : 0;

    // 3. Filter Categories (Only 0xE and 0xF)
    if (category != CAT_SIM_NOTIF && category != CAT_SYS_NOTIF) {
        return -2; // Ignored category
    }

    // Temporary buffer for number conversions
    char num_buf[16]; 

    // 4. Process Notifications
    if (category == CAT_SYS_NOTIF) { // 0xF
        if (sub_cmd == NOTIF_SYS_UNKNOWN) {
            safe_cat(buffer, "SYS ERR: Unknown Command", max_len);
        }
        else if (sub_cmd == NOTIF_SYS_DEBUG) {
            safe_cat(buffer, "SYS DBG: ", max_len);
            safe_cat_payload(buffer, payload, payload_len, max_len);
        }
        else {
            safe_cat(buffer, "SYS NOTIF: Code ", max_len);
            itoa(sub_cmd, num_buf, 16);
            safe_cat(buffer, num_buf, max_len);
        }
    }
    else if (category == CAT_SIM_NOTIF) { // 0xE
        if (sub_cmd == NOTIF_SIM_LOG) {
            safe_cat(buffer, "SIM LOG: ", max_len);
            safe_cat_payload(buffer, payload, payload_len, max_len);
        }
        else if (sub_cmd == NOTIF_SIM_ALERT) {
             safe_cat(buffer, "SIM ALERT: Code ", max_len);
             // Assume payload contains a 4-byte code, otherwise print sub_cmd
             if (payload_len >= 4) {
                 uint32_t code = unpack_uint32(payload);
                 itoa((int)code, num_buf, 16);
                 safe_cat(buffer, num_buf, max_len);
             } else {
                 itoa(sub_cmd, num_buf, 16);
                 safe_cat(buffer, num_buf, max_len);
             }
        }
        else {
            safe_cat(buffer, "SIM NOTIF: Code ", max_len);
            itoa(sub_cmd, num_buf, 16);
            safe_cat(buffer, num_buf, max_len);
        }
    }

    return 0;
}