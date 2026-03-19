#ifndef _COMMAND_PARSE_H_
#define _COMMAND_PARSE_H_

#include "xil_types.h"
#include "hy_types.h"

#define BYTES_PER_SAMPLE        (8*2*2)
#define DMA_TIMEOUT_COUNTER     100000

void tcp_command_parse(u8* cmd, int cmd_len);
int start_dma_trans(samp_cfg_t * sampCfg);
int start_dma_trans_collect_data(samp_cfg_t * sampCfg);
#endif //#ifndef _COMMAND_PARSE_H_
