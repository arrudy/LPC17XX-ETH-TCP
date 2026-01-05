#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include "lwip/tcp.h"
#include "lwip/tcpip.h"


extern osEventFlagsId_t send_flag;

void tcp_mode_server(void);
int8_t tcp_mode_server_defer(void);

int8_t tcp_mode_client(ip_addr_t *target_ip, uint16_t port);
int8_t tcp_mode_client_defer(ip_addr_t *target_ip, uint16_t port);

int8_t tcp_srv_send_data(void *data);
int8_t tcp_srv_send_data_defer(void *data);

//standalone, creates own thread
void initialize_tcp_srv(osMessageQueueId_t i_q);

//from the thread context
void initialize_tcp_srv_threadctx(osMessageQueueId_t i_q);

#endif