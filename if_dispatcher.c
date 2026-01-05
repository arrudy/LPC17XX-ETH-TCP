#include "if_dispatcher.h"

#include "command.h"
#include "uart_parser.h"
#include "eth_handle.h"
#include "slab_alloc.h"
#include "uart_handle.h"
#include "tcp_server.h"

static osMessageQueueId_t out_queue; //where from the packets are taken

__NO_RETURN static void dispatcher_worker(void *argument)
{
   
  for(;;)
  {
    Command cmd;

    osMessageQueueGet(out_queue, &cmd, NULL, osWaitForever);
    
    if(cmd.interface & IF_UART)
    {
      char buffer[128];
      
      parse_command(cmd.data_ptr, buffer, 128);
      osStatus_t result = uart_printn(buffer);
      if(result != osOK)
        uart2_puts_sys("!ERR command serialize fail\n\r");
    }
    if(cmd.interface & IF_ETH)
    {
      tcp_srv_send_data(cmd.data_ptr);
      
      //push to interface
    }
    
    slab_free(cmd.data_ptr);
  }
}



osStatus_t tick_dispatcher(osMessageQueueId_t out_q)
{
  Command cmd;

  for(int i =0; i < 2; ++i){
    osStatus_t result = osMessageQueueGet(out_q, &cmd, NULL, 0);
    if(result != osOK) return result;
    
    if(cmd.interface & IF_UART)
    {
      static char buffer[128];
      buffer[0] = '\0';
      
      parse_command(cmd.data_ptr, buffer, 128);
      result = uart_printn(buffer);
      if(result != osOK)
        uart2_puts_sys("!ERR command serialize fail\n\r");
    }
    if(cmd.interface & IF_ETH)
    {
      int8_t status = tcp_srv_send_data_defer(cmd.data_ptr);
      if(status == -1)
        uart2_puts_sys("!ERR no TCP connection\n\r");
      else if (status < 0)
        uart2_puts_sys("!ERR unknown TCP error\n\r");
      //push to interface
    }
    
    slab_free(cmd.data_ptr);
  }
  
  
  return osOK;
}


osStatus_t init_if_dispatcher(osMessageQueueId_t out_q)
{
  // Validate parameters
    if ( out_q == NULL) {
        return osErrorParameter;
    }

    out_queue = out_q;

    // Fully initialize thread attributes
    const osThreadAttr_t disp_attr = {
        .name       = "if_dispatch_thread",
        .priority   = osPriorityNormal,
        .stack_size = 512
    };

    // Create thread
    osThreadId_t thread_id = osThreadNew(dispatcher_worker, NULL, &disp_attr);
    if (thread_id == NULL) {
        return osErrorResource;   // Thread creation failed
    }

    return osOK;

}