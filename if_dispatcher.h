#ifndef __IF_DISPATCHER_H
#define __IF_DISPATCHER_H

#include "cmsis_os2.h"                  // ::CMSIS:RTOS2


osStatus_t init_if_dispatcher(osMessageQueueId_t out_q);


osStatus_t tick_dispatcher(osMessageQueueId_t out_q);

#endif