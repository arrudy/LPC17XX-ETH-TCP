#include "eth_handle.h"
#include "string.h"
#include "string_extra.h"
#include "uart_handle.h"


//#include "lwipopts.h"                   // lwIP.lwIP::Network:CORE
#include "ethernetif.h"                 // lwIP.lwIP::Network:Interface:ETH
#include "Driver_ETH_PHY.h"             // ::CMSIS Driver:Ethernet PHY
#include "Driver_ETH_MAC.h"             // ::CMSIS Driver:Ethernet MAC


#include "lwip/raw.h"
#include "lwip/icmp.h"
#include "lwip/timeouts.h"
#include "lwip/inet_chksum.h"
#include "lwip/ip_addr.h"
#include "lwip/dhcp.h"
#include "lwip/timeouts.h"

#include "cmsis_os2.h"

#include "ethif_config.h"

#include "tcp_server.h"


//#include "rtx_os.h"
//#include "FreeRTOS.h"             // ARM::CMSIS:RTOS2:FreeRTOS


#include <stdlib.h>

#define ETH_DEB(x) uart2_puts_sys((x)); //printf("%s", (x))


static struct raw_pcb *icmp_pcb;

static u16_t ping_seq_num = 0;


extern ARM_DRIVER_ETH_PHY Driver_ETH_PHY0; 
static struct netif gnetif;


static osMessageQueueId_t in_queue; //received data is pushed here
static osMessageQueueId_t out_queue;



static u8_t ping_recv(void *arg, struct raw_pcb *pcb,
                    struct pbuf *p, const ip_addr_t *addr)
{
  LWIP_UNUSED_ARG(arg);
  LWIP_UNUSED_ARG(pcb);

  struct icmp_echo_hdr *iecho;
  char numbuf[12];

  if (p->len >= sizeof(struct icmp_echo_hdr)) {
      iecho = (struct icmp_echo_hdr*)p->payload;

      // Check if this is a Reply (Type 0) to a ping WE sent
      if (iecho->type == ICMP_ER) {
          ETH_DEB("Ping reply from ");
          ETH_DEB(ipaddr_ntoa(addr));
          ETH_DEB(", seq=");
          itoa(lwip_ntohs(iecho->seqno), numbuf, 10);
          ETH_DEB(numbuf);
          ETH_DEB("\n");
          
          /* We handled the Reply. Free buffer and return 1 (eaten) */
          pbuf_free(p);
          return 1; 
      }
  }

  /* 
   * CRITICAL FIX:
   * If it wasn't a Reply (e.g. it was an Echo Request / Ping from outside),
   * do NOT free the pbuf and return 0.
   * This allows the packet to fall through to lwIP's icmp_input(),
   * which will generate the Pong.
   */
  return 0; 
}









void ping_send_req_cb(void *arg)
{
  char *ip_str = (char*)arg;
  ip_addr_t target_ip;
  struct pbuf *p;
  struct icmp_echo_hdr *iecho;

  /* Safety check: Ensure PCB exists. 
   * If init was skipped, we can technically create it here, 
   * though it's better to rely on ping_raw_init_cb. */
  if (!icmp_pcb) {
        ETH_DEB("Error: icmp_pcb not initialized. Call ping_raw_init_cb first.\n");
        return;
  }

  /* Parse the IP string argument */
  if (!ipaddr_aton(ip_str, &target_ip)) {
        ETH_DEB("Error: Invalid IP address string: ");
        ETH_DEB(ip_str);
        ETH_DEB("\n");
        return;
    }

  /* Allocate memory for the ICMP packet */
  p = pbuf_alloc(PBUF_IP, sizeof(struct icmp_echo_hdr), PBUF_RAM);
  if (!p) {
        ETH_DEB("Error: Failed to allocate pbuf\n");
        return;
  }

  /* Fill the ICMP Header */
  iecho = (struct icmp_echo_hdr*)p->payload;
  ICMPH_TYPE_SET(iecho, ICMP_ECHO);
  ICMPH_CODE_SET(iecho, 0);
  iecho->id = lwip_htons(0x1234);     // Arbitrary ID
  iecho->seqno = lwip_htons(++ping_seq_num);

  /* Calculate Checksum */
  iecho->chksum = 0;
  iecho->chksum = inet_chksum(iecho, sizeof(*iecho));

  char numbuf[16];
  /* Send the packet */
  err_t err = raw_sendto(icmp_pcb, p, &target_ip);
  if (err != ERR_OK) {
    ETH_DEB("Failed to send: ");
    itoa(err, numbuf, 16);  // convert error code to string
    ETH_DEB(numbuf);
    ETH_DEB("\n");
} else {
    ETH_DEB("Ping sent to ");
    ETH_DEB(ip_str);
    ETH_DEB(", seq=");
    itoa(ping_seq_num, numbuf, 10); // convert seq number to string
    ETH_DEB(numbuf);
    ETH_DEB("\n");
}
  /* 
   * CRITICAL: Free the pbuf. 
   * raw_sendto does not consume the pbuf reference. 
   * If you don't free it here, you leak memory. 
   */
  pbuf_free(p);
}





#define LWIP_INIT_FLAG 1U
#define DHCP_INIT_FLAG 2U
#define PING_INIT_FLAG 4U
#define TCP_INIT_FLAG 8U
//lock before the stack is initialized
osEventFlagsId_t eth_init_flags;




static void ping_thread(void *arg)
{
    LWIP_UNUSED_ARG(arg);

    for (;;) {
        /* Ask TCP/IP thread to send a ping */
        tcpip_callback(ping_send_req_cb, "192.168.1.1");

        /* Wait 1 second */
        osDelay(1000);
    }
}




/**
* Initialize the ping recv path & the transmit path
*/
static void ping_raw_init_cb(void *arg)
{
  LWIP_UNUSED_ARG(arg);

  if (icmp_pcb != NULL) {
      ETH_DEB("Ping PCB already initialized.\n");
      return;
  }

  icmp_pcb = raw_new(IP_PROTO_ICMP);
  if (!icmp_pcb) {
      ETH_DEB("Ping raw_new failed\n");
      return;
  }

  /* Bind to IP_ADDR_ANY so we receive packets from ANY interface/IP */
  raw_bind(icmp_pcb, IP_ADDR_ANY);

  /* Register the receive callback (from our previous discussion) */
  raw_recv(icmp_pcb, ping_recv, NULL);

  ETH_DEB("Ping PCB initialized and listening.\n");
  
  /* Optional: Signal your RTOS that init is done */
  osEventFlagsSet(eth_init_flags, PING_INIT_FLAG);
}




// end of ping-removable


#ifndef IP_ADDR0
/* Static IP address */
#define IP_ADDR0                    192
#define IP_ADDR1                    168
#define IP_ADDR2                    1
#define IP_ADDR3                    96
#endif

#ifndef NETMASK_ADDR0
/* NET mask*/
#define NETMASK_ADDR0               255
#define NETMASK_ADDR1               255
#define NETMASK_ADDR2               255
#define NETMASK_ADDR3               0
#endif

#ifndef GW_ADDR0
/* Gateway address */
#define GW_ADDR0                    192
#define GW_ADDR1                    168
#define GW_ADDR2                    1
#define GW_ADDR3                    1
#endif

   






void set_fallback_ip_cb(void *ctx) {
  struct netif *nif = (struct netif *)ctx;
  ip_addr_t ip, mask, gw;
 /* IP4_ADDR(&ip, 169, 254, 0, 121);
  IP4_ADDR(&mask, 255, 255, 255, 0);
  IP4_ADDR(&gw, 169, 254, 0, 1);*/
	IP4_ADDR(&ip, IP_ADDR0	, IP_ADDR1, IP_ADDR2, IP_ADDR3);
  IP4_ADDR(&mask, NETMASK_ADDR0, NETMASK_ADDR1, NETMASK_ADDR2, NETMASK_ADDR3);
  IP4_ADDR(&gw, GW_ADDR0, GW_ADDR1, GW_ADDR2, GW_ADDR3);
  
  netif_set_addr(nif, &ip, &mask, &gw);
  // Also bring it up if it went down
  netif_set_up(nif); 
  //netif_set_link_up(nif); 
}

void netif_status_cb(struct netif *netif) {
    if (dhcp_supplied_address(netif)) {
        //printf("DHCP OK! GOT IP: %s\n", ip4addr_ntoa(netif_ip4_addr(netif)));
        osEventFlagsSet(eth_init_flags, DHCP_INIT_FLAG);
    }
}


static void dhcp_start_cb(void *arg)
{
  (void)dhcp_start((struct netif *)arg);
	osEventFlagsSet(eth_init_flags,DHCP_INIT_FLAG);
}


void lwip_init_cb(void *arg) {
  osEventFlagsSet(eth_init_flags, LWIP_INIT_FLAG);
}




static void netw(void *args) {
  struct netif *netif = (struct netif *) args;
  for (;;) {
    
    LOCK_TCPIP_CORE();
    ethernetif_check_link(netif);
    ethernetif_poll(netif);
    UNLOCK_TCPIP_CORE();
    osDelay(1);
  }
}









void status_callback(struct netif *netif)
{
   if (netif_is_up(netif))
    { 
      printf("status_callback==UP, local interface IP is %s\n", ip4addr_ntoa(netif_ip4_addr(&gnetif)));
   }
   else
   {
       printf("status_callback==DOWN\n");
   }
}

void link_callback(struct netif *netif)
{
   if (netif_is_link_up(netif))
   {
       printf("link_callback==UP\n");
   }
   else
   {
       printf("link_callback==DOWN\n");
   }
}





__NO_RETURN static void eth_init_worker(void *argument)
{
//
//
// lwIP + Eth setup
//
//
  eth_init_flags = osEventFlagsNew(NULL);
  
  

  ETH_DEB("INIT START\n\r");
  
  
  tcpip_init(lwip_init_cb, NULL); //do NOT call lwip_init, as we run in RTOS!
  osEventFlagsWait(eth_init_flags, LWIP_INIT_FLAG, osFlagsWaitAll, osWaitForever);
  ETH_DEB("LWIP OK\n\r");
  
  LOCK_TCPIP_CORE();
  netif_add(&gnetif, 
	 IP4_ADDR_ANY, IP4_ADDR_ANY, IP4_ADDR_ANY,
          NULL, ethernetif_init, tcpip_input);  
  
  PIN_Configure(1, 14, 0U, 3U, 0U);
  
  netif_set_default(&gnetif);
  
  netif_set_status_callback(&gnetif, status_callback);
  netif_set_link_callback(&gnetif, link_callback);

  netif_set_up(&gnetif);
  
  UNLOCK_TCPIP_CORE(); 
  

  
  

  
  {
    ETH_DEB("NETIF OK\n\r");
    
    int retry_count = 0;
    while(Driver_ETH_PHY0.GetLinkState() != ARM_ETH_LINK_UP) {
      osDelay(100);
      retry_count++;
      if(retry_count % 10 == 0) ETH_DEB("."); 
      
      if(retry_count > 50) {
         ETH_DEB("NO CABLE DETECTED\n\r");
        osThreadExit();
        //return; 
      }
    }
    
    ETH_DEB("PHY LINK OK\n\r");
  }
  
  
  osThreadAttr_t netw_attr = {
        .name = "netw_thread",
        .priority = osPriorityAboveNormal,
        .stack_size = 512,
    };
  osThreadNew(netw, &gnetif, &netw_attr);

  

  
  
	
  

//
//
// DHCP start
//
//
	//LOCK_TCPIP_CORE 	( 		);
  //dhcp_start(&gnetif);
  err_t err = tcpip_callback(dhcp_start_cb, &gnetif); 
  
  if (err != ERR_OK) {
    printf("MBOX FULL: %d\n", err); // If this happens, tcpip_thread is definitely stuck
  }
  
	//(void)dhcp_start(&gnetif);
	//UNLOCK_TCPIP_CORE 	( 		);
	
	//osEventFlagsWait(eth_init_flags, DHCP_INIT_FLAG, osFlagsWaitAll, osWaitForever);
	
  

  
  uint32_t flags = osEventFlagsWait(eth_init_flags, DHCP_INIT_FLAG, osFlagsWaitAll, osWaitForever);
  
  ETH_DEB("DHCP INIT OK\n\r");
  osDelay(9000);
  
    
    if (!dhcp_supplied_address(&gnetif)) {
        ETH_DEB("!DHCP FAIL\n\r");
        LOCK_TCPIP_CORE();
        dhcp_stop(&gnetif);
        set_fallback_ip_cb(&gnetif); // Apply your static IP
        UNLOCK_TCPIP_CORE();
    }
    else{
      ETH_DEB("DHCP SUCCESS\n\r");
    }
	
    {
      char ip_buf[32];
      sprintf(ip_buf, "FIN IP: %s\n\r", ip4addr_ntoa(netif_ip4_addr(&gnetif)));
      ETH_DEB(ip_buf);
    }
  
  
    
    
  //
  //
  // Ping Setup
  //
  //




  // Run the PCB creation in the lwIP tcpip_thread
  err_t res = tcpip_callback(ping_raw_init_cb, NULL);
  osEventFlagsWait(eth_init_flags, PING_INIT_FLAG,osFlagsWaitAll, osWaitForever);
  

    
    
    
  //
  //
  // TCP server setup
  //
  //
    
    
        
 initialize_tcp_srv_threadctx(in_queue);
    
 osEventFlagsSet(eth_init_flags, ETH_RDY);
 osThreadExit();
}









osStatus_t initialize_eth_int(osMessageQueueId_t in_q, osMessageQueueId_t out_q)
{
   if (in_q == NULL || out_q == NULL) {
    return osErrorParameter;
  }
  
  in_queue = in_q;
	out_queue = out_q;
  
  	osThreadAttr_t eth_attr = {
        .name = "eth_init_thread",
        .priority = osPriorityNormal,
        .stack_size = 2560,
    };
    
	if (osThreadNew(eth_init_worker,NULL,&eth_attr) == NULL)
    return osErrorResource;
    
  return osOK;
}





















































































#include "RTE_Components.h"
//#include  CMSIS_device_header
#include "cmsis_os2.h"

#include <stdio.h>
#include <stdint.h>

#include "ethernetif.h"
#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/timeouts.h"
#include "netif/etharp.h"
#include "lwip/dhcp.h"


static struct netif netif;

/* Initialize lwIP */
static void net_init (void) {
  ip4_addr_t ipaddr;
  ip4_addr_t netmask;
  ip4_addr_t gw;

  lwip_init();

//#if LWIP_DHCP
//  ipaddr.addr  = IPADDR_ANY;
//  netmask.addr = IPADDR_ANY;
//  gw.addr      = IPADDR_ANY;
//#else
  IP4_ADDR(&ipaddr,  192,168,1,96);
  IP4_ADDR(&netmask, 255,255,255,0);
  IP4_ADDR(&gw,      192,168,1,1);
//#endif

  /* Add your network interface to the netif_list. */
  netif_add(&netif, &ipaddr, &netmask, &gw, NULL, &ethernetif_init, &ethernet_input);

  /* Register the default network interface. */
  netif_set_default(&netif);
  netif_set_up(&netif);

//#if LWIP_DHCP
//  dhcp_start (&netif);
//#endif
}

/* Link check and LED blink */
static void net_periodic (uint32_t tick) {
  static ip4_addr_t ip = {1};
  static uint32_t old_tick = 0U;

  uint32_t PingTimer = 0;
  
  if (tick == old_tick) {
    return;
  }
  old_tick = tick;

  ethernetif_check_link(&netif);
  if (netif_is_link_up(&netif)) {
    /* Print IP address on terminal */
    if (ip.addr != netif.ip_addr.addr) {
      ip.addr = netif.ip_addr.addr;
      printf("IP: %s\n", ipaddr_ntoa(&ip));
    }
  } else {
    if (ip.addr != IPADDR_ANY) {
      ip.addr = IPADDR_ANY;
      printf("Link Down...\n");
    }
  }
  
  
  if( tick - PingTimer >= 5)
  {
   ping_send_req_cb("192.168.1.1");
    PingTimer = tick;
  }
}

/* Tick timer callback */
static void net_timer (uint32_t *tick) {
  *tick += 1U;
}

/*----------------------------------------------------------------------------
 * Application main thread
 *---------------------------------------------------------------------------*/

__NO_RETURN static void eth_worker (void *argument) {
  osTimerId_t id;
  uint32_t tick;
  (void)argument;

  /* Create tick timer, tick interval = 100ms */
  id = osTimerNew((osTimerFunc_t)&net_timer, osTimerPeriodic, &tick, NULL);
  osTimerStart(id, 100U);

  net_init();
  
  ping_raw_init_cb(NULL);
  
  

  /* Infinite loop */
  while (1) {
    /* check if any packet received */
    ethernetif_poll(&netif);
    /* handle system timers for lwIP */
    sys_check_timeouts();
    net_periodic(tick);
  }
}

void initialize_eth_int_fallback (void) {

	osThreadAttr_t eth_attr = {
        .name = "eth_thread",
        .priority = osPriorityNormal,
        .stack_size = 2048,
    };
  osThreadNew(eth_init_worker, NULL, &eth_attr);     // Create application main thread

}






