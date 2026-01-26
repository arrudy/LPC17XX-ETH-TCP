#ifndef __IF_DISPATCHER_H
#define __IF_DISPATCHER_H

#include <cmsis_os2.h>            // ::CMSIS:RTOS2
#include "uart_serializer.h"
#include "uart_handle.h"
#include "slab_alloc.h"
#include "command.h"
#include <string.h> 


osStatus_t init_if_dispatcher(osMessageQueueId_t out_q);


osStatus_t tick_dispatcher(osMessageQueueId_t out_q);

#endif