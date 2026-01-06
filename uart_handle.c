#include "uart_handle.h"
#include "slab_alloc.h"
#include "uart_serializer.h"
//#include "rtx_os.h"

//#include "freertos_mpool.h"             // ARM::CMSIS:RTOS2:FreeRTOS
#include "cmsis_os2.h"                  // ::CMSIS:RTOS2

#include <stdio.h>

//#pragma import(__use_no_semihosting)
//__asm(".global __use_no_semihosting");

#define RX_COMPLETE (1<<ARM_USART_EVENT_RECEIVE_COMPLETE)
#define TX_COMPLETE (1<<ARM_USART_EVENT_SEND_COMPLETE)


osEventFlagsId_t uart_flags;
osMutexId_t uart_if_mtx;


static osMessageQueueId_t in_queue;
static osMessageQueueId_t out_queue;


void UART_signal_event(uint32_t event)
{
	switch(event)
	{
		case ARM_USART_EVENT_RECEIVE_COMPLETE:
			osEventFlagsSet(uart_flags, RX_COMPLETE);
		break;
    case ARM_USART_EVENT_SEND_COMPLETE:
      osEventFlagsSet(uart_flags, TX_COMPLETE);
    break;
		default:
		break;
	}
}

#define MAX_PRINTN 2046


osStatus_t uart_printn(const char *buf)
{
    if (!buf) return osErrorParameter;

    osStatus_t status;
    size_t len = strnlen(buf, MAX_PRINTN); // Protect against non-terminated strings

    if (len == 0) return osOK; // Nothing to send

    // 1. Acquire Mutex to ensure exclusive access to the UART
    // Wait forever (or a long timeout) so we don't drop messages under load
    status = osMutexAcquire(uart_if_mtx, osWaitForever);
    if (status != osOK) return status;

    // 2. Clear the TX flag BEFORE sending to ensure we wait for *this* transmission
    osEventFlagsClear(uart_flags, TX_COMPLETE);

    // 3. Start the Hardware Transfer
    // Note: This returns immediately (non-blocking)
    int32_t driver_status = Driver_USART1.Send(buf, len);

    if (driver_status == ARM_DRIVER_OK) {
      // 4. Block this thread until the ISR sets the TX_COMPLETE flag
      // The RTOS will switch to other threads while the hardware works.
      uint32_t flags = osEventFlagsWait(uart_flags, TX_COMPLETE, osFlagsWaitAny, osWaitForever);
      
      if (flags & osFlagsError) {
          status = osErrorTimeout; // or other error
      } else {
          status = osOK;
      }
    } else {
      // Driver failed to accept data (e.g., busy or error)
      status = osErrorResource;
    }

    // 5. Always Release the Mutex
    osMutexRelease(uart_if_mtx);

    return status;
}


void uart2_putc_sys(int c)
{
    while(!(LPC_UART2->LSR & (1 << 5)));
    
    LPC_UART2->THR = c;
}

void uart2_puts_sys(const char * buf)
{
  for(int i = 0; i < MAX_PRINTN; ++i)
  {
    if(buf[i] == '\0') return;
    uart2_putc_sys(buf[i]);
  }
}



/*
int fputc(int ch, FILE *f)
{
  uart2_putc_sys(ch);
  return ch;
}

int _write(int file, const char *ptr, size_t len)
{
    for (size_t i = 0; i < len; i++)
        uart2_putc_sys(ptr[i]);
    return len;
}*/

//void uart_printn_raw(const char * buf)
//{
//  for(uint32_t i = 0; i < MAX_PRINTN; ++i)
//  {
//    /* Stop if we hit end of string */
//    if(buf[i] == '\0')
//      break;
//      
//    /* Wait until THR is empty (LSR bit 5) */
//    while(!(LPC_UART1->LSR & (1 << 5)));
//    
//    /* Push the byte */
//    LPC_UART1->THR = buf[i];
//  }
//}*/



#define UART_IRQ_PRIORITY  5

void initialize_uart_per()
{
	Driver_USART1.Initialize(UART_signal_event);
	//Power up the USART peripheral /    
	Driver_USART1.PowerControl(ARM_POWER_FULL);
	//Configure the USART to 4800 Bits/sec /    
	Driver_USART1.Control(ARM_USART_MODE_ASYNCHRONOUS | ARM_USART_DATA_BITS_8 | ARM_USART_PARITY_NONE | ARM_USART_STOP_BITS_1 | ARM_USART_FLOW_CONTROL_NONE, 115200);
	// Enable Receiver and Transmitter lines /    
	Driver_USART1.Control (ARM_USART_CONTROL_TX, 1);
	Driver_USART1.Control (ARM_USART_CONTROL_RX, 1);
  
	NVIC_SetPriority(UART1_IRQn, UART_IRQ_PRIORITY);
	NVIC_EnableIRQ(UART1_IRQn);
	
  //const char welcome_msg [] = "UART Ready! Say hiii <3\n\r";
  //uart_printn_raw(welcome_msg);
	//Driver_USART1.Send(welcome_msg, strlen(welcome_msg));
	//while(Driver_USART1.GetTxCount() < strlen(welcome_msg));
  
  PIN_Configure(0,10, 1, 2,0);
	PIN_Configure(0,11, 1, 0,0); //PIN_Configure(0,11, 1, 2,0); //RX pull-up enabled
	LPC_SC->PCONP |= 1<<24;
	LPC_UART2->LCR = 3 | (1<<7); //latch. 8 bits, 1 parity
  
  LPC_UART2->DLL = 10;            // low byte of DL
  LPC_UART2->DLM = 0;             // high byte of DL
  // fractional divider
  LPC_UART2->FDR = (5 << 0)  // DIVADDVAL = 5 (bits 0-3)
                   | (14 << 4); // MULVAL = 14 (bits 4-7)
                   
  
	LPC_UART2->LCR = 3; //de-latch
  
  uart2_puts_sys("USART_if ready\n\r");
}





#define BUF_SIZE 196

__NO_RETURN void uart_worker(void *argument)
{
  const char * welcome_msg = "UART Ready! Hiii <3\n\r";
  uart_printn(welcome_msg);
	
	
  static char in_buffer[BUF_SIZE];
  uint32_t idx = 0;
  osStatus_t status;

  // Arm first receive
  Driver_USART1.Receive(&in_buffer[0], 1);

  for (;;) {

    // wait for one char received
  
    uint32_t state = osEventFlagsWait(uart_flags, RX_COMPLETE, osFlagsWaitAll, osWaitForever);

    // read as much as possible
    if(state == osFlagsErrorTimeout)
    {
      osDelay(5);
      continue;
    }
    
    while (1) {

      idx++;

      // check termination
      char c = in_buffer[idx-1];
      if (c == '\n' || c == '\r' || c == '\0') {
    
        if (idx < BUF_SIZE) {
            in_buffer[idx] = '\0'; 
        } else {
            in_buffer[BUF_SIZE-1] = '\0';
        }
		//strcat(in_buffer, "\n\r");
        //upon input -> TODO, put it into serializer, allocate a slab & push to the queue
        //uart_printn(in_buffer); //loopback placeholder
        
        if(idx > 3) //ignore commands of just termination chars
        {
          Command in_cmd = {
            .interface = IF_UART,
            .data_ptr = serialize_command_alloc(in_buffer, NULL)
          };
          
          osMessageQueuePut(in_queue, &in_cmd, NULL, osWaitForever);
        }
        idx = 0;
        break;
      }

      if (idx >= BUF_SIZE) idx = 0;

      // arm next byte
      Driver_USART1.Receive(&in_buffer[idx], 1);

      // wait a very short time ? collect burst data
      state = osEventFlagsWait(uart_flags, RX_COMPLETE, osFlagsWaitAll, 2);

      if (state == osFlagsErrorTimeout)
          break;  // no more available chars
    }
    //idx = 0;
    // arm next character
    Driver_USART1.Receive(&in_buffer[idx], 1);
  }
}



osStatus_t initialize_uart_int(osMessageQueueId_t in_q, osMessageQueueId_t out_q)
{
  if (in_q == NULL || out_q == NULL) {
    return osErrorParameter;
  }
  
  in_queue = in_q;
	out_queue = out_q;
	
	uart_flags = osEventFlagsNew(NULL);
  if (uart_flags == NULL) {
    return osErrorResource;  // Failed to create event flags
  }
  
	uart_if_mtx = osMutexNew(NULL);
  
  if (uart_if_mtx == NULL) {
    osEventFlagsDelete(uart_flags); // cleanup previous allocation
    uart_flags = NULL;
    return osErrorResource;  // Failed to create mutex
  }
  
  osThreadAttr_t uart_attr = {
        .name = "uart_thread",
        .priority = osPriorityNormal,
        .stack_size = 320,
    };
  
  if (osThreadNew(uart_worker, NULL, &uart_attr) == NULL) {
    osMutexDelete(uart_if_mtx);
    uart_if_mtx = NULL;
    osEventFlagsDelete(uart_flags);
    uart_flags = NULL;
    return osErrorResource;  // Failed to create thread
  }

  return osOK;
}