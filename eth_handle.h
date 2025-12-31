#ifndef ETH_HANDLER_H
#define ETH_HANDLER_H

#include "Driver_ETH_MAC.h"             // CMSIS Ethernet MAC driver
#include "Driver_ETH_PHY.h"             // CMSIS Ethernet PHY driver

#include "lwip/init.h"                  // lwip_init() (if needed)
#include "lwip/tcpip.h"                 // tcpip_init(), tcpip_callback
#include "lwip/netif.h"                 // struct netif, netif_add
#include "lwip/timeouts.h"              // sys_check_timeouts (optional)
#include "lwip/ip_addr.h"               // IP4_ADDR, ip_addr_t
#include "netif/ethernet.h"             // ethernet_input
#include "ethernetif.h"                 // ethernetif_init() (your port)

#include "lwip/raw.h"                   // raw_new(), raw_sendto
#include "lwip/icmp.h"                  // ICMP types
#include "lwip/inet_chksum.h"           // inet_chksum()
#include "lwip/pbuf.h"                  // pbuf_alloc(), pbuf_free



#define ETH_RDY      256U
extern osEventFlagsId_t eth_init_flags;




void ping_send_req_cb(void * ip_addr_str);

osStatus_t initialize_eth_int(osMessageQueueId_t in_q, osMessageQueueId_t out_q);

void initialize_eth_int_fallback(void);


#endif
