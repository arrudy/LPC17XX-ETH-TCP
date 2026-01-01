#ifndef __SLAB_ALLOC_H
#define __SLAB_ALLOC_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <cmsis_os2.h>                // ::CMSIS:RTOS2

// --- Configuration ---

//buffer sizes
#define SIZE_32    32
#define SIZE_128   128
#define SIZE_512   512
#define SIZE_2K    2048


//buffer counts (max 32)
#define COUNT_32   32
#define COUNT_128  8
#define COUNT_512  4
#define COUNT_2K   2





// --- Public API ---

void slab_init(void);

void* slab_malloc(size_t size);
void* slab_calloc(size_t num, size_t size);
void slab_free(void* ptr);




#endif
