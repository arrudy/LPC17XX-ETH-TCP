#include <zephyr/kernel.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/logging/log.h>
#include <dk_buttons_and_leds.h>
#include <string.h>
#include "uart_serializer.h" // Tutaj mamy definicje kodów i unpack_header
#include "protocol.h"        // Tutaj mamy definicje InterfaceType

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

#define BUF_SIZE 256
#define MSGQ_SIZE 10

// === DEFINICJA STRUKTURY WIADOMOŚCI ===
// Musi być widoczna przed definicją kolejki!
typedef struct {
    uint8_t *data;        // Wskaźnik do dynamicznie alokowanego bufora
    size_t len;           // Długość danych
    InterfaceType source; // Skąd przyszło? (IF_CLI lub IF_UART3)
} PacketMsg;

// UART0 (Górny port) -> DANE (Protokół Binarny)
const struct device *uart_data = DEVICE_DT_GET(DT_NODELABEL(uart0));

// USB CDC (Boczny port) -> KONSOLA (Tekst)
const struct device *uart_cli = DEVICE_DT_GET(DT_NODELABEL(cdc_acm_uart0));

// Definicja kolejki z użyciem naprawionej struktury
K_MSGQ_DEFINE(main_msgq, sizeof(PacketMsg), MSGQ_SIZE, 4);

// Bufor dla Konsoli (Tekst)
static char cli_line_buf[BUF_SIZE];
static int  cli_line_pos = 0;

// === ISR: Konsola (USB - Tekst) ===
static void cli_isr(const struct device *dev, void *user_data) {
    uint8_t c;
    uart_irq_update(dev);
    if (uart_irq_rx_ready(dev)) {
        while (uart_fifo_read(dev, &c, 1) == 1) {
            uart_poll_out(dev, c); // Echo
            if (c == '\n' || c == '\r') {
                if (cli_line_pos > 0) {
                    cli_line_buf[cli_line_pos] = '\0';
                    size_t len;
                    // Serializacja tekstu do binarki
                    uint8_t* pkt = serialize_command_alloc(cli_line_buf, &len);
                    if (pkt) {
                        PacketMsg msg = { .data = pkt, .len = len, .source = IF_CLI };
                        k_msgq_put(&main_msgq, &msg, K_NO_WAIT);
                    } else {
                        uart_poll_out(dev, '?'); uart_poll_out(dev, '\n');
                    }
                    cli_line_pos = 0;
                }
                uart_poll_out(dev, '\r'); uart_poll_out(dev, '\n');
            } else {
                if (cli_line_pos < BUF_SIZE - 1) cli_line_buf[cli_line_pos++] = c;
            }
        }
    }
}
// === ISR: DANE (UART0 - Binarny) ===
static void data_isr(const struct device *dev, void *user_data) {
    uint8_t c;
    
    // Bufor strumieniowy
    static uint8_t rx_buf[256];
    static int rx_pos = 0;
    static uint16_t total_frame_len = 0; // Ile bajtów łącznie musimy uzbierać (Header + Payload)

    uart_irq_update(dev);

    if (uart_irq_rx_ready(dev)) {
        while (uart_fifo_read(dev, &c, 1) == 1) {
            
            // 1. Zbieramy dane do bufora
            if (rx_pos < sizeof(rx_buf)) {
                rx_buf[rx_pos++] = c;
            } else {
                // Przepełnienie - reset
                rx_pos = 0; 
                total_frame_len = 0;
            }

            // 2. Analiza nagłówka (gdy mamy pierwsze 4 bajty)
            if (total_frame_len == 0 && rx_pos >= 4) {
                uint16_t payload_len_from_header;
                uint16_t func_code;
                
                // Odczytujemy to co jest w nagłówku (np. 8)
                unpack_header(rx_buf, &payload_len_from_header, &func_code);
                
                // === KLUCZOWA POPRAWKA ===
                // Protokół mówi: Wartość w nagłówku to PAYLOAD.
                // Więc całkowita długość ramki to: PAYLOAD + 4 BAJTY NAGŁÓWKA.
                total_frame_len = payload_len_from_header + 4;
                
                // Walidacja (czy nie za duże na nasz bufor)
                if (total_frame_len > sizeof(rx_buf) || total_frame_len < 4) {
                    rx_pos = 0; 
                    total_frame_len = 0;
                }
            }

            // 3. Czy mamy całą ramkę?
            if (total_frame_len > 0 && rx_pos >= total_frame_len) {
                // Mamy komplet! (np. 12 bajtów: 4 header + 8 payload)
                
                uint8_t* pkt_copy = k_malloc(total_frame_len);
                if (pkt_copy) {
                    memcpy(pkt_copy, rx_buf, total_frame_len);
                    
                    PacketMsg msg = { 
                        .data = pkt_copy, 
                        .len = total_frame_len, // Przekazujemy CAŁĄ długość ramki
                        .source = IF_UART3 
                    };
                    k_msgq_put(&main_msgq, &msg, K_NO_WAIT);
                }

                // Obsługa sklejonych ramek (jeśli przyszło coś więcej niż ta jedna ramka)
                if (rx_pos > total_frame_len) {
                    int leftover = rx_pos - total_frame_len;
                    memmove(rx_buf, &rx_buf[total_frame_len], leftover);
                    rx_pos = leftover;
                } else {
                    rx_pos = 0;
                }
                total_frame_len = 0; // Reset, czekamy na nowy nagłówek
            }
        }
    }
}

int main(void) {
    k_msleep(100);
    dk_leds_init();

    if (usb_enable(NULL) != 0) { return -1; }
    k_msleep(1000); 

    if (!device_is_ready(uart_data) || !device_is_ready(uart_cli)) return -1;

    // Konfiguracja przerwań
    uart_irq_callback_user_data_set(uart_cli, cli_isr, NULL);
    uart_irq_rx_enable(uart_cli);
    uart_line_ctrl_set(uart_cli, UART_LINE_CTRL_DTR, 1);

    uart_irq_callback_user_data_set(uart_data, data_isr, NULL);
    uart_irq_rx_enable(uart_data);

    LOG_INF("SYSTEM READY");
    LOG_INF(" - CLI (Text/Cmd): USB CDC");
    LOG_INF(" - DATA (Binary): UART0");

    PacketMsg msg;
    for (;;) {
        if (k_msgq_get(&main_msgq, &msg, K_FOREVER) == 0) {
            dk_set_led(DK_LED1, 1);

            if (msg.source == IF_UART3) { 
                // Przyszło z SIECI (UART0)
                uint16_t payload_len, func;
                
                // unpack_header odczyta z bajtów wartość "8"
                unpack_header(msg.data, &payload_len, &func);
                
                LOG_INF("RECV FRAME: HeaderVal=%d, TotalRx=%d, Func=0x%03X", 
                        (int)payload_len, (int)msg.len, func);
                
                // Wypisujemy Payload
                // msg.len to całkowita długość (Header+Payload), np. 12
                // Odejmujemy 4 bajty nagłówka, zostaje 8 bajtów danych.
                size_t actual_payload_len = msg.len - 4;

                if (actual_payload_len > 0) {
                    uint8_t *payload_ptr = msg.data + 4; // Przesuwamy wskaźnik za nagłówek

                    // HEX DUMP
                    LOG_HEXDUMP_INF(payload_ptr, actual_payload_len, "Payload (HEX)");

                    // // STRING VIEW
                    // char txt_buf[129];
                    // size_t print_len = (actual_payload_len < 128) ? actual_payload_len : 128;
                    // memcpy(txt_buf, payload_ptr, print_len);
                    // txt_buf[print_len] = '\0';
                    // LOG_INF("Payload (Str): %s", txt_buf);
                }

                // Echo dla debugu na konsolę CLI
                uart_poll_out(uart_cli, '[');
                uart_poll_out(uart_cli, 'R');
                uart_poll_out(uart_cli, 'X');
                uart_poll_out(uart_cli, ']');
                uart_poll_out(uart_cli, '\r');
                uart_poll_out(uart_cli, '\n');

            } else {
                // Przyszło z KONSOLI -> WYSYŁAMY DO SIECI
                // Uwaga: Tutaj serialize_command_alloc musi też być zgodny z tą logiką!
                // W obecnej wersji serialize_command_alloc wpisuje TOTAL size. 
                // Jeśli chcesz być w 100% zgodny, musisz w uart_serializer.c zmienić:
                // pack_header(..., (uint16_t)payload_size, ...); zamiast total_size.
                
                for(int i=0; i<msg.len; i++) uart_poll_out(uart_data, msg.data[i]);
                LOG_INF("CMD -> NET (%d bytes)", (int)msg.len);
            }

            if (msg.data) k_free(msg.data);
            dk_set_led(DK_LED1, 0);
        }
    }
}