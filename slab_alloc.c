#include "slab_alloc.h"





// Alignment ensures these buffers can hold structs safely
#define ALIGNMENT _Alignas(16) 

// --- Static Storage ---

/*ALIGNMENT*/ static uint8_t pool_32[COUNT_32][SIZE_32];
/*ALIGNMENT*/ static uint8_t pool_128[COUNT_128][SIZE_128];
/*ALIGNMENT*/ static uint8_t pool_512[COUNT_512][SIZE_512];
/*ALIGNMENT*/ static uint8_t pool_2k[COUNT_2K][SIZE_2K];

// --- Availability Flags (1 = Used, 0 = Free) ---

static uint32_t flags_32 = 0;
static uint32_t flags_128 = 0;
static uint32_t flags_512 = 0;
static uint32_t flags_2k = 0;



static osMutexId_t slab_mutex_id;
// Reserve static memory for the Mutex Control Block.
// Must be 64-bit aligned on Cortex-M3 to avoid Usage Faults.
/*ALIGNMENT*/ static uint64_t slab_mutex_cb[8]; 

static const osMutexAttr_t slab_mutex_attr = {
    .name = "SlabAllocatorMutex",
    // CRITICAL: Priority Inheritance prevents high-priority tasks from
    // blocking on this allocator if a low-priority task is preempted.
    .attr_bits = osMutexPrioInherit, 
    .cb_mem = &slab_mutex_cb,
    .cb_size = sizeof(slab_mutex_cb)
};



void slab_init(void) {
    slab_mutex_id = osMutexNew(&slab_mutex_attr);
    // Note: If using RTX5, ensure osKernelInitialize() is called before this.
}



// --- Helper Functions ---

// Returns index of first 0 bit, or -1 if all are 1
static int find_free_index(uint32_t mask, int max_count) {
  
  if(mask == 0xFFFFFFFF) return -1; //optimization
  
    for (int i = 0; i < max_count; i++) {
        if (!((mask >> i) & 1)) {
            return i;
        }
    }
    return -1;
}



/**
  * @brief Finds the first free bit (0) using Cortex-M3 hardware instructions.
 * @return Index (0-31) or -1 if full.
 *//*
__STATIC_FORCEINLINE int find_free_index(uint32_t mask, int max_count) {
    // 1. Invert mask: We are looking for a '0' in the original, 
    //    so we look for a '1' in the inverted value.
    uint32_t free_bits = ~mask;

    // 2. Check if pool is totally full (no 1s left)
    if ((free_bits & ((1ULL << max_count) - 1)) == 0) {
        return -1;
    }

    // 3. Cortex-M3 Optimization:
    // __RBIT reverses bits (MSB <-> LSB).
    // __CLZ counts leading zeros.
    // This combination effectively finds the index of the Least Significant Set Bit (CTZ).
    // Example: 00...0100 (Index 2) -> RBIT -> 0010...00 -> CLZ -> 2.
    return __CLZ(__RBIT(free_bits));
}
*/









static void set_flag(uint32_t *mask, int index, int used) {
    if (used) *mask |= (1U << index);
    else      *mask &= ~(1U << index);
}


// --- Public API ---

void* slab_malloc(size_t size) {
    if (size == 0 || size > 2048) return NULL;

    int start_tier = 0;
    if (size > 512) start_tier = 3;
    else if (size > 128) start_tier = 2;
    else if (size > 32)  start_tier = 1;

    void* ptr = NULL;

    // Acquire Mutex (Waits forever, Priority Inherited)
    osStatus_t status = osMutexAcquire(slab_mutex_id, osWaitForever);
    if (status != osOK) return NULL;

    // Cascade / Fallback Search
    for (int tier = start_tier; tier <= 3; tier++) {
        int index = -1;

        switch (tier) {
            case 0: // 32 Bytes
                index = find_free_index(flags_32, COUNT_32);
                if (index != -1) {
                    set_flag(&flags_32, index, 1);
                    ptr = &pool_32[index][0];
                }
                break;
            case 1: // 128 Bytes
                index = find_free_index(flags_128, COUNT_128);
                if (index != -1) {
                    set_flag(&flags_128, index, 1);
                    ptr = &pool_128[index][0];
                }
                break;
            case 2: // 512 Bytes
                index = find_free_index(flags_512, COUNT_512);
                if (index != -1) {
                    set_flag(&flags_512, index, 1);
                    ptr = &pool_512[index][0];
                }
                break;
            case 3: // 2048 Bytes
                index = find_free_index(flags_2k, COUNT_2K);
                if (index != -1) {
                    set_flag(&flags_2k, index, 1);
                    ptr = &pool_2k[index][0];
                }
                break;
        }

        if (ptr != NULL) break;
    }

    osMutexRelease(slab_mutex_id);
    return ptr;
}

void* slab_calloc(size_t num, size_t size) {
    size_t total_size = num * size;
    if (num != 0 && total_size / num != size) return NULL; // Overflow check

    void* ptr = slab_malloc(total_size);
    if (ptr) {
        memset(ptr, 0, total_size);
    }
    return ptr;
}

void slab_free(void* ptr) {
    if (!ptr) return;
    uintptr_t p_addr = (uintptr_t)ptr;

    uint32_t* target_flags = NULL;
    int target_index = -1;

    // Address arithmetic does NOT require lock (Read-Only static constants)
    if (p_addr >= (uintptr_t)pool_32 && p_addr < (uintptr_t)pool_32 + sizeof(pool_32)) {
        size_t offset = p_addr - (uintptr_t)pool_32;
        if (offset % SIZE_32 == 0) {
            target_flags = &flags_32;
            target_index = offset / SIZE_32;
        }
    }
    else if (p_addr >= (uintptr_t)pool_128 && p_addr < (uintptr_t)pool_128 + sizeof(pool_128)) {
        size_t offset = p_addr - (uintptr_t)pool_128;
        if (offset % SIZE_128 == 0) {
            target_flags = &flags_128;
            target_index = offset / SIZE_128;
        }
    }
    else if (p_addr >= (uintptr_t)pool_512 && p_addr < (uintptr_t)pool_512 + sizeof(pool_512)) {
        size_t offset = p_addr - (uintptr_t)pool_512;
        if (offset % SIZE_512 == 0) {
            target_flags = &flags_512;
            target_index = offset / SIZE_512;
        }
    }
    else if (p_addr >= (uintptr_t)pool_2k && p_addr < (uintptr_t)pool_2k + sizeof(pool_2k)) {
        size_t offset = p_addr - (uintptr_t)pool_2k;
        if (offset % SIZE_2K == 0) {
            target_flags = &flags_2k;
            target_index = offset / SIZE_2K;
        }
    }

    if (target_flags != NULL && target_index != -1) {
        osMutexAcquire(slab_mutex_id, osWaitForever);
        set_flag(target_flags, target_index, 0);
        osMutexRelease(slab_mutex_id);
    }
}
