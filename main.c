#include "RTE_Components.h"
#include CMSIS_device_header
#include "Board_LED.h"                  // ::Board Support:LED
#include "cmsis_os2.h"
#include "Board_Buttons.h"              // ::Board Support:Buttons
#include "rtx_os.h"
//#include "FreeRTOS.h"
#include "Driver_USART.h"               // ::CMSIS Driver:USART
#include "string.h"
#include "string_extra.h"
#include "PIN_LPC17xx.h"                // Keil::Device:PIN
#include "eth_handle.h"

#include "uart_handle.h"
#include "command.h"
#include "layer_void.h"

//#pragma import(__use_no_semihosting)


osEventFlagsId_t evtFlags;



void EINT0_IRQHandler(void)
{
  osEventFlagsSet(evtFlags, 1);
  LPC_SC->EXTINT |= 1;
}




void HardFault_Handler(void)
{
	LED_Initialize();
	LED_SetOut(0);
	int led_state = 0;
	while(1)
	{
	for(uint32_t i = 0; i < 10000000; ++i){;}
		led_state = !led_state; //? 0 : 1;
		if(led_state) {LED_On(0); LED_On(2);}
    
		else {LED_Off(0); LED_Off(2);}
		
	}

}



uint32_t osRtxErrorNotify(uint32_t code, void * object_id)
{
	switch(code)
		{
		case osRtxErrorStackUnderflow:
    {
      const char * text = "Stack overflow\n\r";
      uart_printn(text);
      printf("%s",text);
      //USARTdrv->Send(text, strlen(text));
      //return 0;
      break;
    }
		case osRtxErrorISRQueueOverflow:
		{
			const char * text = "ISR queue full\n\r";
      uart_printn(text);
      printf("%s",text);
			//USARTdrv->Send(text, strlen(text));
			//return 0;
      break;
		}
    default:
    {
      const char * text = "Unknown RTX5 error\n\r";
      uart_printn(text);
      printf("%s",text);
      break;
    }
	}
  
  for (;;) {}
}

osMessageQueueId_t in_q = NULL;
osMessageQueueId_t out_q = NULL;




#define POST_INIT 1
#define RUNNING 2




__NO_RETURN void app_main(void *argument)
{
	
  //malloc(10);
  uint32_t state = 0;
  
  
  
	int led_state = 0;
	for(;;)
	{
		
		led_state = !led_state; //? 0 : 1;
		if(led_state) LED_On(3);
		else LED_Off(3);
    
    
    switch(state)
    {
      case 0:
      {
        uint32_t flag_res = osEventFlagsWait(eth_init_flags, ETH_RDY, osFlagsWaitAll, 0);
        
        if( !(flag_res & 1<<31) && (flag_res & ETH_RDY)) state = POST_INIT;
        else{  osDelay(500); }
          
      }
      break;
      case 1:
      {
        (void)initialize_uart_int(in_q, out_q);
        //(void)init_pipeline(in_q, out_q);
        state = RUNNING;
        osDelay(1000);
      }
      break;
      default:
        tcpip_callback(ping_send_req_cb, "192.168.1.1");
        osDelay(2000);
      break;
    }
    
    
    
    
    
    /*
    printf("P: %d, C: %d, Stat: 0x%X\n", 
        LPC_EMAC->TxProduceIndex, 
        LPC_EMAC->TxConsumeIndex, 
        LPC_EMAC->Status);
    
    printf("TX Proc: %d, Cons: %d\n", LPC_EMAC->TxProduceIndex, LPC_EMAC->TxConsumeIndex);
    
    printf("RX Prod: %d, Cons: %d, Stat: 0x0%X\n", 
        LPC_EMAC->RxProduceIndex, 
        LPC_EMAC->RxConsumeIndex, 
        LPC_EMAC->Status);*/
	}
}


int main(void)
{
	SystemCoreClockUpdate();
	LED_Initialize();
	for(uint32_t i = 0; i < 10000000; ++i){;}
	
	
	/*
	Buttons_Initialize();
	PIN_Configure(2,10, 1, 0,0);
	LPC_SC->EXTMODE |= 1;
	LPC_SC->EXTPOLAR = 0;
	LPC_SC->EXTINT = 0;
    NVIC_ClearPendingIRQ(EINT0_IRQn);
	NVIC_EnableIRQ(EINT0_IRQn);*/
	LED_SetOut(0);

	
	for(int i = 0; i < 10000000; ++i);
  
	initialize_uart_per();
	
	
	
	(void)osKernelInitialize();
  
    in_q = osMessageQueueNew(10, sizeof(Command), NULL);
	  out_q = osMessageQueueNew(10, sizeof(Command), NULL);
	
  
  
  (void)initialize_eth_int();
  
  
  
	osThreadAttr_t main_attr = {
        .name = "main_thread",
        .priority = osPriorityNormal,
        .stack_size = 256,
    };
	
    
	(void)osThreadNew(app_main,NULL,&main_attr);
	
 
	//(void)evtFlags = osEventFlagsNew(NULL); //enable for button interrupt input
	
    if (osKernelGetState() == osKernelReady) {
      osKernelStart();
    }
    else
    {
      printf("KERNEL FAILURE");
    }

	
	for(;;)
	{
		
	}
	//return 0;
}
