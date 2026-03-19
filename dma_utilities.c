#include "xaxidma.h"
#include "dma_utilities.h"

XAxiDma axiDma;

volatile int dma_error;
volatile int dma_rx_done;
volatile int dma_all_done;


int axiDma_init()
{
    int Status;
    XAxiDma_Config *config;
    
    config = XAxiDma_LookupConfig(DMA_DEV_ID);
    if (!config) {
        xil_printf("No config found for %d\r\n", DMA_DEV_ID);
        return XST_FAILURE;
    }

    Status = XAxiDma_CfgInitialize(&axiDma, config);
    if (Status != XST_SUCCESS) {
        xil_printf("Initialization failed %d\r\n", Status);
        return XST_FAILURE;
    }

    dma_rx_done = 0;
    dma_error   = 0;

    return XST_SUCCESS;
}

int axiDma_reset()
{
	int timeout;
	XAxiDma *axidma_inst = &axiDma;

    XAxiDma_Reset(axidma_inst);
    timeout = RESET_TIMEOUT_COUNTER;
    while (timeout) {
        if (XAxiDma_ResetIsDone(axidma_inst))
            break;
        timeout -= 1;
    }

    XAxiDma_IntrEnable(axidma_inst, XAXIDMA_IRQ_ALL_MASK, XAXIDMA_DEVICE_TO_DMA);

    return 0;
}

void rx_intr_handler(void *callback)
{
    u32 irq_status;
    int timeout;
    XAxiDma *axidma_inst = (XAxiDma *) callback;

    irq_status = XAxiDma_IntrGetIrq(axidma_inst, XAXIDMA_DEVICE_TO_DMA);
    XAxiDma_IntrAckIrq(axidma_inst, irq_status, XAXIDMA_DEVICE_TO_DMA);

    if ((irq_status & XAXIDMA_IRQ_ERROR_MASK)) {
        dma_error = 1;
        XAxiDma_Reset(axidma_inst);
        timeout = RESET_TIMEOUT_COUNTER;
        while (timeout) {
            if (XAxiDma_ResetIsDone(axidma_inst))
                break;
            timeout -= 1;
        }
        return;
    }

    if ((irq_status & XAXIDMA_IRQ_IOC_MASK))
        dma_rx_done = 1;
}

int axiDma_transfer_dev2dma(UINTPTR *BufferAddr, u32 Length)
{
    int Status;
    Status = XAxiDma_SimpleTransfer(&axiDma, (UINTPTR)BufferAddr, Length, XAXIDMA_DEVICE_TO_DMA);
    if (Status != XST_SUCCESS)
    {
    	xil_printf("dma fail\r\n");
        return XST_FAILURE;
    }
}
