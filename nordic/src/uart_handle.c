#include <cmsis_os2.h>
#include <zephyr/kernel.h>
#include <nrfx_uarte.h>
#include <string.h>

#include "uart_handle.h"
#include "uart_parser.h"      // Musi zawierać deklarację uart_parser_feed
#include "uart_serializer.h"  // Musi zawierać deklarację uart_serialize
#include "command.h"

// --- Konfiguracja Pinów ---
#define TX_PIN_NUMBER  6
#define RX_PIN_NUMBER  8

static const nrfx_uarte_t m_uart = NRFX_UARTE_INSTANCE(0);

// --- Zmienne ---
static uint8_t rx_byte; // Bufor na 1 bajt
static osMessageQueueId_t *ptr_in_q = NULL;
static osMessageQueueId_t *ptr_out_q = NULL;
osThreadId_t uart_tx_tid = NULL;

// --- Callback ---
void uart_event_handler(nrfx_uarte_event_t const * p_event, void * p_context)
{
    switch (p_event->type)
    {
        case NRFX_UARTE_EVT_RX_DONE:
        {
            // Sprawdzamy pole .rx.bytes (w nowym nrfx)
            if (p_event->data.rx.length > 0) {
                if (ptr_in_q != NULL) {
                    uart_parser_feed(rx_byte, *ptr_in_q); 
                }
            }
            // Ponowne uzbrojenie odbioru 1 bajtu
            nrfx_uarte_rx(&m_uart, &rx_byte, 1);
            break;
        }

        case NRFX_UARTE_EVT_TX_DONE:
        {
            if (uart_tx_tid != NULL) {
                osThreadFlagsSet(uart_tx_tid, 0x01);
            }
            break;
        }

        case NRFX_UARTE_EVT_ERROR:
        {
            // W razie błędu wznawiamy odbiór
            nrfx_uarte_rx(&m_uart, &rx_byte, 1);
            break;
        }
        
        default: break;
    }
}

// --- Wątek TX ---
__NO_RETURN void uart_tx_thread(void *arg) {
    Command cmd;
    uint8_t tx_temp_buf[128];

    for(;;) {
        if (ptr_out_q && osMessageQueueGet(*ptr_out_q, &cmd, NULL, osWaitForever) == osOK) {
            
            // Serializacja
            int len = uart_serialize(&cmd, tx_temp_buf, sizeof(tx_temp_buf));
            
            if (len > 0) {
                nrfx_uarte_tx(&m_uart, tx_temp_buf, len, 0);
                osThreadFlagsWait(0x01, osFlagsWaitAll, osWaitForever);
            }
            
            if (cmd.data_ptr) {
                 slab_free(cmd.data_ptr);
            }
        }
    }
}

// --- Init ---
int initialize_uart_int(osMessageQueueId_t in_q, osMessageQueueId_t out_q)
{
    static osMessageQueueId_t static_in_q;
    static osMessageQueueId_t static_out_q;
    static_in_q = in_q;
    static_out_q = out_q;

    ptr_in_q = &static_in_q;
    ptr_out_q = &static_out_q;

    // Ręczna konfiguracja zgodna z nowym nrfx
    nrfx_uarte_config_t config = {0};
    

    config.txd_pin = TX_PIN_NUMBER; 
    config.rxd_pin = RX_PIN_NUMBER;
    
    config.baudrate = NRF_UARTE_BAUDRATE_115200;
    config.interrupt_priority = 6;
    
    // Inicjalizacja
    if (nrfx_uarte_init(&m_uart, &config, uart_event_handler) != NRFX_SUCCESS) {
        return -1;
    }

    // Start odbioru
    nrfx_uarte_rx(&m_uart, &rx_byte, 1);

    // Start wątku TX
    osThreadAttr_t tx_attr = {
        .name = "UART_TX",
        .priority = osPriorityNormal,
        .stack_size = 1024
    };
    uart_tx_tid = osThreadNew(uart_tx_thread, NULL, &tx_attr);

    return 0;
}

osStatus_t uart_printn(const char* str) {
    nrfx_uarte_tx(&m_uart, (uint8_t*)str, strlen(str), 0);
    // Delay prymitywny
    for(volatile int i=0; i<10000; i++); 
    return osOK;
}