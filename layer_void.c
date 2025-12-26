#include "layer_void.h"
#include "layer_cmd.h"
#include "layer_sim.h"
#include "uart_serializer.h"



static osMessageQueueId_t in_queue;
static osMessageQueueId_t out_queue;


/**
* Process the command
* Since it is the last layer, its output is irrelevant
* We need to use a queue here in order to sent something to one of the interfaces
*/
int process_request_void(Command * cmd_in)
{
  
  Command cmd_out = { 
    .interface = cmd_in->interface,
    .data_ptr = serialize_command_alloc("debug send unknown_command", NULL),
  };
  
  osStatus_t result;
  result = osMessageQueuePut(out_queue, NULL,NULL, osWaitForever);
  if(osErrorResource == result)
  {
    osDelay(100);
    result = osMessageQueuePut(out_queue, NULL,NULL, osWaitForever);
  }
  
  return 1;
}



static __NO_RETURN void pipeline_worker(void *argument)
{
  int (*const func_ptr[3])(Command *) = {process_request_cmd, process_request_sim, process_request_void};
	
	for(;;)
	{
    Command cmd;
    osMessageQueueGet(in_queue, &cmd, NULL, osWaitForever);
    
    for(int i = 0; i < sizeof(func_ptr)/sizeof(*func_ptr); ++i)
    {
      if(func_ptr[i](&cmd)) break;
    }
    slab_free(cmd.data_ptr);
  }
}



osStatus_t init_pipeline(osMessageQueueId_t in_q, osMessageQueueId_t out_q)
{
    // Validate parameters
    if (in_q == NULL || out_q == NULL) {
        return osErrorParameter;
    }

    in_queue  = in_q;
    out_queue = out_q;

    // Fully initialize thread attributes
    const osThreadAttr_t pipe_attr = {
        .name       = "PipelineThread",
        .priority   = osPriorityNormal,
        .stack_size = 1024
    };

    // Create thread
    osThreadId_t thread_id = osThreadNew(pipeline_worker, NULL, &pipe_attr);
    if (thread_id == NULL) {
        return osErrorResource;   // Thread creation failed
    }

    return osOK;
}
