#ifndef __COMMAND_H
#define __COMMAND_H

#include <stdint.h>
#include <stddef.h>

//source definitions
#define IF_INTERNAL    0x01
#define IF_UART        0x02
#define IF_ETH         0x04
//#define IF_P2P         0x08
//#define IF_BROAD_INNER 0x7F // nearest neighbor broadcast; UART user, P2P and server
//#define IF_BROAD_OUTER 0xFF // every single node in the web, including UART


// --- Command Definitions (Same as before) ---
#define CAT_SYSTEM    0x1
#define CAT_SIM       0x2
#define CAT_SIM_API   0x3
#define CAT_SIM_NOTIF 0xE
#define CAT_SYS_NOTIF 0xF





/*
  
 OUTPUTS  

*/


// System commands
#define SYS_PING     0x01 //payload: str(ip)
#define SYS_CONN     0x02 //payload: str(ip)
#define SYS_DISCONN  0x03 //payload: None
#define SYS_RAW_SEND 0x04 //payload: str(any)



// System Notifications (0xF)
#define SYS_NOTIF_UNKNOWN 0x01 //payload: None
#define SYS_NOTIF_DEBUG   0x02 //payload: str(any)
#define SYS_NOTIF_FORBID  0x03 //payload: None
#define SYS_NOTIF_OK      0x04 //payload: None

// Simulation Notifications (0xE)
#define NOTIF_SIM_ALERT   0x01 // Code Payload
#define NOTIF_SIM_LOG     0x02 // String Payload







typedef struct {
  uint8_t interface; //source or target, depending on where it is fed
  uint8_t * data_ptr; //typically a pointer to a slab
} Command;



void pack_header(uint8_t* buffer, uint16_t length, uint16_t func_code, uint8_t flags);
int unpack_header(const uint8_t * buffer, uint16_t * len, uint16_t * func_code, uint8_t * flags);
int parse_line(char *line, Command *cmd);


#endif
