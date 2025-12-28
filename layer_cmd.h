#ifndef __LAYER_CMD_H
#define __LAYER_CMD_H

#include "command.h"
#include "uart_serializer.h"
#include "eth_handle.h"
#include "uart_handle.h"
#include "tcp_server.h"

static char ip [] = "255.255.255.255";


int process_request_cmd(Command * cmd)
{ 
  
  uint16_t len, func;
  uint8_t flags;
  
  unpack_header(cmd->data_ptr, &len, &func, &flags);
  
  uint8_t cat = (func>>8) & 0xF;
  uint8_t sub_cmd = func &  0xFF;
  
  if( cat != CAT_SYSTEM ) return 0; 
  
  
  switch( sub_cmd )
  {
    case SYS_PING:
      strcpy(ip, (const char*) (cmd->data_ptr+4));
      tcpip_callback(ping_send_req_cb, ip);
    break;
    case SYS_CONN:
      strcpy(ip, (const char*) (cmd->data_ptr+4));
      ip_addr_t target_ip;
    
      if (!ipaddr_aton(ip, &target_ip)) {
          uart2_puts_sys("Error: Invalid IP address string: ");
          uart2_puts_sys(ip);
          uart2_puts_sys("\n");
          return 1;
      }
      
      tcp_mode_client(&target_ip, 5000);
    break;
    case SYS_DISCONN:
      tcp_mode_server();
    break;
    case SYS_RAW_SEND:
     tcp_srv_send_data_defer(cmd->data_ptr);
    break;
  }
  
  
  
  return 1;
}



#endif
