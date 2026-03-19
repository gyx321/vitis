#ifndef __DMA_UTILITIES_H__
#define __DMA_UTILITIES_H__

#include "hy_types.h"
#include "xil_types.h"
#include "xparameters.h"

#define DMA_DEV_ID              XPAR_AXIDMA_0_DEVICE_ID
#define DMA_RX_INTR_ID          XPAR_FABRIC_AXIDMA_0_VEC_ID

#define DDR_BASE_ADDR           XPAR_PSU_DDR_0_S_AXI_BASEADDR
#define MEM_BASE_ADDR           (DDR_BASE_ADDR + 0x10000000)
#define RX_BUFFER_BASE    	    (MEM_BASE_ADDR + 0x00000000)
#define TCP_BUFFER_BASE			(MEM_BASE_ADDR + 0x10000000)

#define RESET_TIMEOUT_COUNTER   10000
// #define DMA_PKT_LEN             32768 * 256 / 8

int axiDma_init();
int axiDma_reset();
void rx_intr_handler(void *callback);
int axiDma_transfer_dev2dma(UINTPTR *BufferAddr, u32 Length);

#endif
