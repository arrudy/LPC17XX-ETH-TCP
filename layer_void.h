#ifndef __LAYER_VOID_H
#define __LAYER_VOID_H

#include "command.h"
#include "slab_alloc.h"

#include "cmsis_os2.h"
//#include "rtx_os.h"
//#include "FreeRTOS.h"                   // ARM.FreeRTOS::RTOS:Core



osStatus_t init_pipeline(osMessageQueueId_t in_q, osMessageQueueId_t out_q);



#endif
