/* uart_handle.h - Wersja dla nRF52840 */
#ifndef UART_HANDLE_H
#define UART_HANDLE_H

#include <cmsis_os2.h>      
#include <zephyr/kernel.h>
#include <nrfx_uarte.h>
#include <string.h>

#include "uart_parser.h"      // Naprawia błąd: implicit declaration of uart_parser_feed
#include "uart_serializer.h"  // Naprawia błąd: implicit declaration of uart_serialize
#include "command.h"

// Usuwamy specyficzne nagłówki LPC/Keil (RTE_Components.h, CMSIS_device_header)

// Prototypy funkcji
int initialize_uart_int(osMessageQueueId_t in_q, osMessageQueueId_t out_q);
osStatus_t uart_printn(const char* str);

#endif // UART_HANDLE_H