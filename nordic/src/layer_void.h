#ifndef __LAYER_VOID_H
#define __LAYER_VOID_H

#include "command.h"
#include "slab_alloc.h"

#include <cmsis_os2.h>



osStatus_t init_pipeline(osMessageQueueId_t in_q, osMessageQueueId_t out_q);



#endif
