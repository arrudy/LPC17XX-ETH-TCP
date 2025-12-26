#ifndef __IF_DISPATCHER_H
#define __IF_DISPATCHER_H

#include "command.h"
#include "uart_parser.h"
#include "eth_handle.h"
#include "slab_alloc.h"



extern osMessageQueueId_t out_queue; //where from the packets are taken

__NO_RETURN void dispatcher_worker(void *argument)
{
   
  for(;;)
  {
    Command cmd;

    osMessageQueueGet(out_queue, &cmd, NULL, osWaitForever);
    
    if(cmd.interface & CMD_UART)
    {
      char buffer[128];
      
      parse_command(cmd.data_ptr, buffer, 128);
      osStatus_t result = uart_printn(buffer);
      if(result != osOK)
        uart_printn_raw(buffer);
    }
    if(cmd.interface & CMD_ETH);
    {
      //push to interface
    }
    
    slab_free(cmd.data_ptr);
  }
}


#endif