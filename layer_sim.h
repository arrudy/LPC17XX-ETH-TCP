#ifndef __LAYER_SIM_H
#define __LAYER_SIM_H

#include "command.h"
#include "uart_serializer.h"

/*
struct Tamagotchi {
  int32_t health = 100;
  int32_t energy = 100;
} zwierze;*/




int process_request_sim(Command * cmd)
{ 
  
  uint16_t len, func;
  uint8_t flags;
  
  unpack_header(cmd->data_ptr, &len, &func, &flags);
  if(func != CAT_SIM && func != CAT_SIM_API) return 0; 
  
	return 1;
}



#endif
