#include "cmsis_os2.h"
#include <string.h>
#include "command.h"    // User header
#include "slab_alloc.h" // User header
#include "tcp_server.h"

// --- GLOBAL STATE ---
static osMessageQueueId_t g_in_q = NULL;

typedef struct {
    ip_addr_t ip;
    uint16_t port;
} client_connect_ctx_t;

osEventFlagsId_t send_flag;
static volatile int8_t volatile_send_status = 0;
static volatile int8_t volatile_tcp_conn = 0;

static struct tcp_pcb *current_pcb = NULL;     // The active data connection (Client or Server link)
static struct tcp_pcb *server_pcb = NULL;      // The Listener (Server mode only)
static struct pbuf *rx_backlog = NULL;         // RX reassembly chain

// --- PROTOTYPES ---
static void cleanup_connection(struct tcp_pcb *pcb);
static struct pbuf* pbuf_consume_bytes(struct pbuf *head, u16_t bytes_to_eat);
static int8_t _start_listener_raw(void);

// ============================================================================
// 1. HELPER: Pbuf Chain Management & Listener Logic
// ============================================================================

/**
 * Internal Helper: Starts listening on Port 5000.
 * Assumes LOCK_TCPIP_CORE is already held.
 */
static int8_t _start_listener_raw(void)
{
    if (server_pcb != NULL) return 0; // Already listening

    struct tcp_pcb *pcb = tcp_new();
    if (pcb == NULL) return -1;

    // Bind to Server Port (Fixed 5000 as per previous context)
    err_t err = tcp_bind(pcb, IP_ADDR_ANY, 5000); 
    
    if (err != ERR_OK) {
        tcp_close(pcb);
        return -2;
    }

    server_pcb = tcp_listen(pcb);
    // Forward declarations for callbacks are needed if compiled strictly, 
    // but assuming standard C scope, we define callbacks below. 
    // We register the callback in the public functions or accept callback usually,
    // but here we need to register the accept callback immediately.
    // (See section 2 for implementation of tcp_accept_cb)
    extern err_t tcp_accept_cb(void *arg, struct tcp_pcb *newpcb, err_t err);
    tcp_accept(server_pcb, tcp_accept_cb);
    
    return 0;
}

static struct pbuf* pbuf_consume_bytes(struct pbuf *head, u16_t bytes_to_eat)
{
    struct pbuf *p = head;
    struct pbuf *temp;

    while (p != NULL && bytes_to_eat > 0) {
        if (bytes_to_eat >= p->len) {
            bytes_to_eat -= p->len;
            temp = p;
            p = p->next;
            temp->next = NULL;
            pbuf_free(temp); 
        } else {
            pbuf_header(p, -(s16_t)bytes_to_eat);
            bytes_to_eat = 0;
        }
    }
    return p;
}

/**
 * Cleans up a specific connection and handles State Machine transitions.
 */
static void cleanup_connection(struct tcp_pcb *pcb)
{
    // 1. Standard Resource Cleanup
    if (pcb != NULL) {
        tcp_arg(pcb, NULL);
        tcp_sent(pcb, NULL);
        tcp_recv(pcb, NULL);
        tcp_err(pcb, NULL);
        tcp_close(pcb);
    }

    if (current_pcb == pcb) {
        current_pcb = NULL;
    }

    if (rx_backlog != NULL) {
        pbuf_free(rx_backlog);
        rx_backlog = NULL;
    }

    // 2. STATE MACHINE: Auto-Revert to Listener
    // If we have no active connection AND no listener (server_pcb),
    // it means we were in Client Mode and got disconnected.
    // We must revert to listening to avoid being unreachable.
    if (server_pcb == NULL) {
        _start_listener_raw();
    }
}

// ============================================================================
// 2. LWIP CALLBACKS (Run in lwIP Thread)
// ============================================================================

// Error / Reset / Abort
static void tcp_err_cb(void *arg, err_t err)
{
    // pcb is already freed by lwIP. Just clear global reference.
    current_pcb = NULL;
    if (rx_backlog) {
        pbuf_free(rx_backlog);
        rx_backlog = NULL;
    }

    // STATE MACHINE: Auto-Revert
    // If we were a client (server_pcb is null) and crashed, revert to listener.
    if (server_pcb == NULL) {
        _start_listener_raw();
    }
}

// Data Receive
static err_t tcp_recv_cb(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    if (p == NULL) {
        // Remote host closed connection
        cleanup_connection(tpcb);
        return ERR_OK;
    }

    if (err != ERR_OK) {
        pbuf_free(p);
        return err;
    }

    // 1. Add to backlog
    if (rx_backlog == NULL) {
        rx_backlog = p;
    } else {
        pbuf_cat(rx_backlog, p);
    }

    // 2. Process complete packets
    while (rx_backlog != NULL) {
        
        if (rx_backlog->tot_len < 4) break; // Need header

        uint8_t hdr_buf[4];
        pbuf_copy_partial(rx_backlog, hdr_buf, 4, 0);

        uint16_t packet_len = 0;
        uint16_t f_code = 0;
        uint8_t flags = 0;

        unpack_header(hdr_buf, &packet_len, &f_code, &flags);
        if (packet_len == 0) packet_len = 4; // Safety

        if (rx_backlog->tot_len < packet_len) break; // Wait for full packet

        // 3. Alloc Slab & Copy
        void *slab_data = slab_malloc(packet_len);
        if (slab_data != NULL) {
            pbuf_copy_partial(rx_backlog, slab_data, packet_len, 0);

          
            Command cmd_out = { 
              .interface = IF_ETH,
              .data_ptr = slab_data
            };
            
            // Push to Queue
            if (osMessageQueuePut(g_in_q, &cmd_out, 0, 0) != osOK) {
                slab_free(slab_data); // Queue full
            }
        }

        // 4. Consume & Ack
        rx_backlog = pbuf_consume_bytes(rx_backlog, packet_len);
        tcp_recved(tpcb, packet_len);
    }

    return ERR_OK;
}

// Accept (Server Mode)
err_t tcp_accept_cb(void *arg, struct tcp_pcb *newpcb, err_t err)
{
    if (err != ERR_OK || newpcb == NULL) return ERR_VAL;

    // Kill old connection if exists (Zombie/Reconnection handling)
    if (current_pcb != NULL) {
        tcp_abort(current_pcb);
        current_pcb = NULL;
        if (rx_backlog) {
            pbuf_free(rx_backlog);
            rx_backlog = NULL;
        }
    }

    current_pcb = newpcb;
    tcp_arg(current_pcb, NULL);
    tcp_recv(current_pcb, tcp_recv_cb);
    tcp_err(current_pcb, tcp_err_cb);
    
    return ERR_OK;
}

// Connected (Client Mode)
static err_t tcp_connected_cb(void *arg, struct tcp_pcb *tpcb, err_t err)
{
    if (err == ERR_OK) {
        current_pcb = tpcb;
    } else {
        // If connection failed, tcp_err_cb likely won't fire for the 'connect' call err
        // We should ensure we revert to listening
        if (server_pcb == NULL) {
            _start_listener_raw();
        }
    }
    return ERR_OK;
}

static void tcp_mode_server_cb_internal(void *ctx)
{
    (void)ctx; // No arguments needed

    // 1. Abort current connection if any
    if (current_pcb != NULL) {
        // Since we are in the LwIP thread, tcp_abort is safe here.
        // It uses the LwIP thread's large stack.
        tcp_abort(current_pcb);
        current_pcb = NULL;
        if (rx_backlog) { pbuf_free(rx_backlog); rx_backlog = NULL; }
    }

    // 2. Start Listening (if not already)
    // Reuse your existing raw helper
    _start_listener_raw();

    // 3. Signal Complete
    // We reuse the same flag group, using bit 0x4 for "Server Mode Set"
    osEventFlagsSet(send_flag, 0x4);
}

// ============================================================================
// 3. PUBLIC TX FUNCTION (Async / Dispatcher Called)
// ============================================================================

int8_t tcp_srv_send_data(void *data)
{
    LOCK_TCPIP_CORE();

    if (current_pcb == NULL) {
        UNLOCK_TCPIP_CORE();
        return -1; // No connection
    }

    // 1. Get Length
    uint16_t packet_len = 0;
    uint16_t f_code = 0;
    //uint8_t flags = 0;
    
    unpack_header((uint8_t*)data, &packet_len, &f_code, NULL);
    if (packet_len == 0) packet_len = 4;

    // 2. Check Buffer Space
    if (tcp_sndbuf(current_pcb) < packet_len) {
        UNLOCK_TCPIP_CORE();
        return -2; // Buffer full
    }

    // 3. Write (Copy Mode)
    err_t err = tcp_write(current_pcb, data, packet_len, TCP_WRITE_FLAG_COPY);

    if (err == ERR_OK) {
        tcp_output(current_pcb);
    }

    UNLOCK_TCPIP_CORE();
    
    return (err == ERR_OK) ? 0 : -3;
}

int8_t tcp_mode_server_defer(void)
{
    // 1. Clear flag
    osEventFlagsClear(send_flag, 0x4);

    // 2. Schedule Callback
    // We pass NULL because we don't need arguments
    err_t err = tcpip_callback(tcp_mode_server_cb_internal, NULL);

    if (err != ERR_OK) {
        return -4; // Queue full
    }

    // 3. Block and Wait
    uint32_t flags = osEventFlagsWait(send_flag, 0x4, osFlagsWaitAny, osWaitForever);

    if (flags & 0x80000000U) {
        return -5; // Error
    }

    return 0; // Success
}




/**
 * 1. The Callback Function
 * This runs INSIDE the LwIP thread (tcpip_thread).
 * The Stack size here is large (as defined in lwipopts.h).
 * NO LOCKS are needed here because we are already in the core thread.
 */
static void tcp_srv_send_callback(void *ctx)
{
    void *data = ctx; // Context is simply the data pointer
    volatile_send_status = 0; // Default to OK

    // Safety check: Is global PCB valid?
    if (current_pcb == NULL) {
        volatile_send_status = -1; // No connection
        osEventFlagsSet(send_flag, 0x1);
        return;
    }

    // 1. Get Length
    uint16_t packet_len = 0;
    uint16_t f_code = 0;
    uint8_t flags = 0;
    
    // Unpack header from the raw data pointer
    unpack_header((uint8_t*)data, &packet_len, &f_code, &flags);
    if (packet_len == 0) packet_len = 4;

    // 2. Check Buffer Space
    if (tcp_sndbuf(current_pcb) < packet_len) {
        volatile_send_status = -2; // Buffer full
        osEventFlagsSet(send_flag, 0x1);
        return;
    }

    // 3. Write (Copy Mode)
    // Note: TCP_WRITE_FLAG_COPY is crucial here because 'data' resides 
    // in the other thread's scope (or slab) and we need LwIP to copy it 
    // to its own memory before we release the semaphore/flag.
    err_t err = tcp_write(current_pcb, data, packet_len, TCP_WRITE_FLAG_COPY);

    if (err == ERR_OK) {
        tcp_output(current_pcb);
        volatile_send_status = 0;
    } else {
        volatile_send_status = -3; // Write failed (e.g. ERR_MEM)
    }

    // 4. Signal completion (Success or Error)
    osEventFlagsSet(send_flag, 0x1);
}

/**
 * 2. The User-Facing Function
 * Call this from your User Thread (with the small stack).
 * It delegates the work to LwIP and sleeps until finished.
 */
int8_t tcp_srv_send_data_defer(void *data)
{
    // 1. Clear the flag to ensure we don't catch a stale event
    osEventFlagsClear(send_flag, 0x1);

    // 2. Schedule the callback
    // tcpip_callback is thread-safe and puts the message in the mbox
    err_t err = tcpip_callback(tcp_srv_send_callback, data);

    if (err != ERR_OK) {
        return -4; // Failed to schedule callback (Queue full?)
    }

    // 3. Block and Wait
    // This puts the user thread to sleep, saving stack and CPU.
    // We wait forever (osWaitForever) or you could set a timeout (e.g., 1000ms)
    uint32_t flags = osEventFlagsWait(send_flag, 0x1, osFlagsWaitAny, osWaitForever);

    if (flags & 0x80000000U) {
        return -5; // OS Error (Timeout or Resource invalid)
    }

    // 4. Return the status set by the callback
    return volatile_send_status;
}




static void client_connect_cb_internal(void *ctx)
{
    client_connect_ctx_t *args = (client_connect_ctx_t*)ctx;
    volatile_tcp_conn = 0; // Assume Success initially

    // 1. Cleanup Old Server/Connection
    if (server_pcb != NULL) {
        tcp_close(server_pcb);
        server_pcb = NULL;
    }
    
    if (current_pcb != NULL) {
        tcp_abort(current_pcb);
        current_pcb = NULL;
        if (rx_backlog) { pbuf_free(rx_backlog); rx_backlog = NULL; }
    }

    // 2. Create New PCB
    struct tcp_pcb *pcb = tcp_new();
    if (pcb == NULL) {
        // Critical Memory Error
        volatile_tcp_conn = -1; 
        _start_listener_raw(); // Revert to listener so we aren't dead
        goto done;
    }

    // 3. Setup Callbacks
    tcp_bind(pcb, IP_ADDR_ANY, 0); 
    tcp_arg(pcb, NULL);
    tcp_recv(pcb, tcp_recv_cb);
    tcp_err(pcb, tcp_err_cb);

    // 4. Initiate Connection
    // Note: This only sends the SYN packet. It does not wait for the ACK.
    // If SYN is sent successfully, we return 0. 
    // The actual connection status comes later in tcp_connected_cb.
    err_t err = tcp_connect(pcb, &args->ip, args->port, tcp_connected_cb);
    
    if (err != ERR_OK) {
        volatile_tcp_conn = -2; // Routing failed or Out of Memory
        _start_listener_raw(); // Revert
    }

done:

    // 6. Signal User Thread
    // We reuse 0x1, assuming the user doesn't try to connect and send 
    // simultaneously from different threads.
    osEventFlagsSet(send_flag, 0x2);
}





// ============================================================================
// 4. PUBLIC STATE CONTROL FUNCTIONS
// ============================================================================

/**
 * Switch to Server Mode (Start Listening).
 * If currently acting as a client, it aborts the connection and starts listening.
 */
void tcp_mode_server(void)
{
    LOCK_TCPIP_CORE();

    // 1. Abort current connection if any
    if (current_pcb != NULL) {
        tcp_abort(current_pcb);
        current_pcb = NULL;
        if (rx_backlog) { pbuf_free(rx_backlog); rx_backlog = NULL; }
    }

    // 2. Start Listening (if not already)
    _start_listener_raw();

    UNLOCK_TCPIP_CORE();
}

/**
 * Switch to Client Mode (Connect to Target).
 * Stops listening and attempts connection.
 */
/* int8_t tcp_mode_client(ip_addr_t *target_ip, uint16_t port) //heavy, only usable in tcpip ctx
{
    LOCK_TCPIP_CORE();

    // 1. Stop Listening (Clean up Server PCB)
    if (server_pcb != NULL) {
        tcp_close(server_pcb);
        server_pcb = NULL;
    }

    // 2. Abort active connection if any
    if (current_pcb != NULL) {
        tcp_abort(current_pcb);
        current_pcb = NULL;
        if (rx_backlog) { pbuf_free(rx_backlog); rx_backlog = NULL; }
    }

    // 3. Initiate Connection
    struct tcp_pcb *pcb = tcp_new();
    if (pcb == NULL) {
        _start_listener_raw(); // Fail-safe: Revert to listener
        UNLOCK_TCPIP_CORE();
        return -1;
    }

    tcp_bind(pcb, IP_ADDR_ANY, 0); 
    tcp_arg(pcb, NULL);
    tcp_recv(pcb, tcp_recv_cb);
    tcp_err(pcb, tcp_err_cb);

    err_t err = tcp_connect(pcb, target_ip, port, tcp_connected_cb);
    
    if (err != ERR_OK) {
        _start_listener_raw(); // Fail-safe
        UNLOCK_TCPIP_CORE();
        return -2;
    }

    UNLOCK_TCPIP_CORE();
    return 0; // Request Started
}*/


int8_t tcp_mode_client_defer(ip_addr_t *target_ip, uint16_t port)
{
    // 1. Clear flag to avoid stale events
    osEventFlagsClear(send_flag, 0x2);

    // 2. Allocate arguments (Using Slab to save Stack)
    client_connect_ctx_t args;

    // Copy data
    args.ip = *target_ip;
    args.port = port;

    // 3. Schedule Callback
    err_t err = tcpip_callback(client_connect_cb_internal, &args);
    if (err != ERR_OK) {
        return -4; // Return immediately, do not wait!
    }


    // 4. Block and Wait
    // The LwIP thread will process the logic and wake us up.
    uint32_t flags = osEventFlagsWait(send_flag, 0x2, osFlagsWaitAny, osWaitForever);

    if (flags & 0x80000000U) {
        return -5; // OS Timeout/Error
    }

    // 5. Return Result
    return volatile_tcp_conn;
}





// ============================================================================
// 5. INITIALIZATION & THREAD
// ============================================================================

const osThreadAttr_t tcp_init_attr = {
    .name = "tcp_init_thread",
    .stack_size = 1024,
    .priority = (osPriority_t) osPriorityNormal,
};

static void tcp_init_thread(void *arg)
{
    // Default Start State: SERVER
    tcp_mode_server();
    
    // Init complete, thread can exit.
    osThreadExit();
}

void initialize_tcp_srv(osMessageQueueId_t i_q)
{
    g_in_q = i_q;
    send_flag = osEventFlagsNew(NULL);
    osThreadNew(tcp_init_thread, NULL, &tcp_init_attr);
}


void initialize_tcp_srv_threadctx(osMessageQueueId_t i_q)
{
    g_in_q = i_q;
  
   send_flag = osEventFlagsNew(NULL);
    tcp_mode_server();
}

