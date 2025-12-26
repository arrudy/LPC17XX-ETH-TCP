#ifndef __LAYER_CMD_H
#define __LAYER_CMD_H

#include "command.h"
#include "uart_serializer.h"

int process_request_cmd(Command * cmd)
{ 
  
  uint16_t len, func;
  uint8_t flags;
  
  unpack_header(cmd->data_ptr, &len, &func, &flags);
  if(func != CAT_SYSTEM) return 0; 
  
  return 1;
}



#endif
