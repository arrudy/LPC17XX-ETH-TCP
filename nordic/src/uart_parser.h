#ifndef __UART_PARSER_H
#define __UART_PARSER_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "command.h"
#include "string_extra.h"
#include <cmsis_os2.h> 

int parse_command(const uint8_t * p, char* buffer, size_t max_len);


#endif