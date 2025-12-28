# ⚡ LPC17XX-ETH-TCP

![MCU](https://img.shields.io/badge/MCU-NXP_LPC17xx-blue)
![Stack](https://img.shields.io/badge/Stack-lwIP_2.3.0-green)
![RTOS](https://img.shields.io/badge/RTOS-CMSIS_RTOS2-orange)
![Compiler](https://img.shields.io/badge/Compiler-ARM_AC6-red)

A robust, thread-safe TCP/IP implementation for the **NXP LPC17xx** microcontroller series, running on **CMSIS-RTOS2** and **lwIP**. 

This project solves common stability issues (Stack Overflows, Hard Faults, and Memory Corruption) associated with the LPC17xx EMAC driver by implementing correct PBUF sizing, thread-context bridging, and proper memory locking.

## 🚀 Capabilities

*   **Dual Mode TCP:** Seamless switching between Server (Listener) and Client modes.
*   **Thread-Safe Dispatching:** Uses `tcpip_callback` to offload TX operations to the high-stack lwIP core thread, allowing user threads to remain lightweight.
*   **Driver-Aligned Buffering:** Optimized `lwipopts.h` configuration specifically tuned for the LPC17xx non-scatter-gather DMA (1536-byte PBUFs).
*   **Zero-Copy Logic:** Implements efficient slab allocation and pointer passing to minimize RAM usage on the constrained 32KB boundary.

## 🛠️ Build Requirements

To compile and run this project, ensure your environment matches the following specifications:

| Component | Version | Notes |
| :--- | :--- | :--- |
| **Compiler** | `ARM Compiler 6 (AC6)` | Required for C11/C++14 support and optimization. |
| **CMSIS** | `5.6.X` (Advised) | Verified stable. Newer versions (6.x) may require RTE adaptation. |
| **lwIP** | `2.3.0` | Sourced via Keil Pack. |
| **IDE** | Keil MDK-ARM | uVision 5.x or newer. |

### 📦 Key Dependencies
*   **Device Family Pack:** NXP LPC1700 Series
*   **CMSIS Driver:** Ethernet MAC (LPC17xx) & PHY (Generic or DP83848)
