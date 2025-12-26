#ifndef UART_HANDLER_H
#define UART_HANDLER_H

#include "RTE_Components.h"
#include CMSIS_device_header
#include "Board_LED.h"                  // ::Board Support:LED
#include "cmsis_os2.h"
#include "Board_Buttons.h"              // ::Board Support:Buttons
//#include "rtx_os.h"
//#include "FreeRTOS.h"                   // ARM.FreeRTOS::RTOS:Core

#include "Driver_USART.h"               // ::CMSIS Driver:USART
#include "string.h"
#include "string_extra.h"
#include "PIN_LPC17xx.h"                // Keil::Device:PIN


extern ARM_DRIVER_USART Driver_USART1;

void initialize_uart_per();
osStatus_t initialize_uart_int(osMessageQueueId_t in_q, osMessageQueueId_t out_q);


osStatus_t uart_printn(const char *buf);
void uart_printn_raw(const char * buf);

void uart2_putc_sys(int c);

#endif
