#ifndef __COMMAND_H
#define __COMMAND_H

#include <stdint.h>

//source definitions
#define CMD_INTERNAL    0x01
#define CMD_UART        0x02
#define CMD_ETH         0x04
#define CMD_P2P         0x08
#define CMD_BROAD_INNER 0x7F // nearest neighbor broadcast; UART user, P2P and server
#define CMD_BROAD_OUTER 0xFF // every single node in the web, including UART


// --- Command Definitions (Same as before) ---
#define CAT_SYSTEM    0x1
#define CAT_SIM       0x2
#define CAT_SIM_API   0x3
#define CAT_SIM_NOTIF 0xE
#define CAT_SYS_NOTIF 0xF





/*
  
 OUTPUTS  

*/


// System Notifications (0xF)
#define NOTIF_SYS_UNKNOWN 0x01
#define NOTIF_SYS_DEBUG   0x02 // String Payload

// Simulation Notifications (0xE)
#define NOTIF_SIM_ALERT   0x01 // Code Payload
#define NOTIF_SIM_LOG     0x02 // String Payload







typedef struct {
  uint8_t interface; //source or target, depending on where it is fed
  uint8_t * data_ptr; //typically a pointer to a slab
} Command;



void pack_header(uint8_t* buffer, uint16_t length, uint16_t func_code, uint8_t flags);
int unpack_header(const uint8_t * buffer, uint16_t * len, uint16_t * func_code, uint8_t * flags);



#endif
