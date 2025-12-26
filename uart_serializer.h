#ifndef __UART_SERIALIZER_H
#define __UART_SERIALIZER_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "slab_alloc.h"
#include "command.h"


#define MAX_TOKENS 10
#define MAX_ARGS   5



// --- Main Serializer with Slab Allocation ---

/**
 * @brief Parses input, allocates memory via slab_malloc, and returns packet.
 * @param input_cmd Null-terminated command string.
 * @param out_len   Pointer to store total length of allocated packet.
 * @return Pointer to slab-allocated memory (caller must slab_free), or NULL on error.
 */
uint8_t* serialize_command_alloc( char* input_cmd, size_t* out_len);

#endif
