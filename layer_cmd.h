#ifndef __LAYER_CMD_H
#define __LAYER_CMD_H

#include "command.h"
#include "uart_serializer.h"
#include "eth_handle.h"
#include "uart_handle.h"
#include "tcp_server.h"


static osMessageQueueId_t in_queue;
static osMessageQueueId_t out_queue;

static char ip [] = "255.255.255.255";


int process_request_cmd(Command * cmd)
{ 
  
  uint16_t len, func;
  uint8_t flags;
  
  unpack_header(cmd->data_ptr, &len, &func, &flags);
  
  uint8_t cat = (func>>8) & 0xF;
  uint8_t sub_cmd = func &  0xFF;
  
  if( cat != CAT_SYSTEM && cat != CAT_SYS_API ) return 0; 
  
  switch(cat)
  {
    case CAT_SYSTEM:
    switch( sub_cmd )
    {
      case SYS_PING:
        //strcpy(ip, (const char*) (cmd->data_ptr+4));
        strncpy(ip, (const char*) (cmd->data_ptr+4), sizeof(ip));
        tcpip_callback(ping_send_req_cb, ip);
      break;
      case SYS_CONN:
        //strcpy(ip, (const char*) (cmd->data_ptr+4));
        strncpy(ip, (const char*) (cmd->data_ptr+4), sizeof(ip));
        ip_addr_t target_ip;
      
        if (!ipaddr_aton(ip, &target_ip)) {
            uart2_puts_sys("Error: Invalid IP address string: ");
            uart2_puts_sys(ip);
            uart2_puts_sys("\n\r");
            return 1;
        }
        
        if(!tcp_mode_client_defer(&target_ip, 5000))
          uart_printn("OK\n\r");
        else
          uart_printn("!ERR\n\r");
      break;
      case SYS_DISCONN:
        if(!tcp_mode_server_defer())
          uart_printn("OK\n\r");
        else
          uart_printn("!ERR\n\r");
      break;
      case SYS_RAW_SEND:
      {
        if(cmd->interface & IF_ETH) //forbidden
        {
          Command cmd_out = {.interface = IF_ETH, .data_ptr = slab_malloc(4)};
          if(!cmd_out.data_ptr)
          {
            uart2_puts_sys("CMD_LYR: alloc failure.");
            return 1;
          }
          pack_header(cmd_out.data_ptr, 4, (CAT_SYS_NOTIF<<8) | SYS_NOTIF_FORBID, 0);
          osMessageQueuePut(out_queue, &cmd_out, NULL, osWaitForever);
          return 1;
        }
          
        
        Command cmd_out = {.interface = IF_ETH, .data_ptr = slab_malloc(len)};
        if(!cmd_out.data_ptr)
        {
          uart2_puts_sys("CMD_LYR: alloc failure.");
          return 1;
        }
        pack_header(cmd_out.data_ptr,len & 0xFFF,func & 0xFFF,flags & 0xFF);
        //strcpy((char*)(cmd_out.data_ptr+4), (const char*) (cmd->data_ptr+4));
        strncpy((char*)(cmd_out.data_ptr+4), (const char*) (cmd->data_ptr+4), len-4);
        
        osMessageQueuePut(out_queue, &cmd_out, NULL, osWaitForever);
       //tcp_srv_send_data_defer(cmd->data_ptr); //fallback
      }
     break;
    }
    break;
    case CAT_SYS_API:
    switch(sub_cmd)
    {
      case SYS_ETH_MSG:
      {
        Command cmd_out = {.interface = IF_UART, .data_ptr = slab_malloc(len)};
        if(!cmd_out.data_ptr)
        {
          uart2_puts_sys("CMD_LYR: alloc failure.");
          return 1;
        }
        pack_header(cmd_out.data_ptr,len & 0xFFF,func & 0xFFF,flags & 0xFF);
        strncpy((char*)(cmd_out.data_ptr+4), (const char*) (cmd->data_ptr+4), len-4);
        osMessageQueuePut(out_queue, &cmd_out, NULL, osWaitForever);
        
      }
      break;
      case SYS_LOOP:
      {        
        Command cmd_out = {.interface = IF_ETH, .data_ptr = slab_malloc(len)};
        if(!cmd_out.data_ptr)
        {
          uart2_puts_sys("CMD_LYR: alloc failure.");
          return 1;
        }
        pack_header(cmd_out.data_ptr,len & 0xFFF,func & 0xFFF,flags & 0xFF);
        //strcpy((char*)(cmd_out.data_ptr+4), (const char*) (cmd->data_ptr+4));
        strncpy((char*)(cmd_out.data_ptr+4), (const char*) (cmd->data_ptr+4), len-4);
        
        osMessageQueuePut(out_queue, &cmd_out, NULL, osWaitForever);
       //tcp_srv_send_data_defer(cmd->data_ptr); //fallback
      }
     break;
      
    }
    break;
  }
  
  
  
  
  
  
  return 1;
}



#endif
