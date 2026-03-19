#include <stdio.h>
#include <stdint.h>
#include "platform.h"
#include "sleep.h"
#include "xtime_l.h"
#include "xaxidma.h"
#include "xscugic.h"
#include "xil_exception.h"
#include "sys_intr.h"
#include "adi_utilities.h"
#include "axi_interface.h"
#include "perip_ctrl.h"
#include "gpio_emio.h"
#include "tcp_utilities.h"
#include "dma_utilities.h"
#include "gpio_emio.h"
#include "command_parse.h"
#include "netif/xadapter.h"
#include "lwip/priv/tcp_priv.h"

//#include "PSDK.h"

extern adi_adrv9025_Device_t adrv9029Device_A;
extern adi_adrv9025_Device_t adrv9029Device_B;

XScuGic intc;

extern XGpioPs GpioPs;

extern XAxiDma axiDma;
extern volatile int dma_all_done;
extern volatile int dma_error;
extern struct netif server_netif;
extern volatile int TcpFastTmrFlag;
extern volatile int TcpSlowTmrFlag;
extern volatile unsigned tcp_client_connected;

extern int op_cmd;
extern op_config_t op_cfg;
extern samp_cfg_t samp_cfg_collect_data;
extern samp_cfg_t samp_cfg;
extern int flag;
extern uint8_t lo_lna_sw;

int main()
{
	int Status;
	int32_t recoveryAction;

	init_platform();

	/* Specify custom platform */
	adi_hal_PlatformSetup(NULL, noos_PLATFORM);

	/* Reset all peripheral */
	PL_RESETALL();

	adi_hal_Wait_ms(NULL, 200);

	/* Initialize GPIO */
	gpio_Init();

	//	通道供电开启
	XGpioPs_WritePin(&GpioPs, EMIO_EN_RFPOWER, 1);
	sleep(2);

	printf("begin\n");

////	通道供电关闭
//	XGpioPs_WritePin(&GpioPs, EMIO_EN_RFPOWER, 0);

	/************************************************************************
	 *                   BEGIN: Initialize ADRV9029 RX                      *
	 ************************************************************************/

	/* Boot up ADRV9029 */
	recoveryAction = adi_adrv9029_BootUp();
	if (recoveryAction != ADI_COMMON_ACT_NO_ACTION)
	{
		printf("ERROR: : Failed to boot up ADRV9029.\n");
		/* Call action handler */
		return ADI_COMMON_ACT_ERR_RESET_FULL;
	}

	/* Bring up JESD RX link */
	recoveryAction = adi_adrv9029_RxBringUp();
	if (recoveryAction != ADI_COMMON_ACT_NO_ACTION)
	{
		printf("ERROR: : Failed to bring up ADRV9029 JESD RX.\n");
		/* Call action handler */
		return ADI_COMMON_ACT_ERR_RESET_FULL;
	}

	/* Set all RX channels enable */
	recoveryAction = adi_adrv9029_RxTxEnableSet(ADI_RXALL, ADI_TXOFF);
	if (recoveryAction != ADI_COMMON_ACT_NO_ACTION)
	{
		printf("Failed to enable ADRV9025_B Rx RF input\n");
		/* Call action handler */
		return ADI_COMMON_ACT_ERR_RESET_FULL;
	}

	/* Set rx LO frequency to 500MHz */
	recoveryAction = adi_adrv9029_PllFreqSet(ADI_ADRV9025_LO1_PLL, 500000000);
	if (recoveryAction != ADI_COMMON_ACT_NO_ACTION)
	{
		printf("Failed to enable ADRV9025_B Rx RF input\n");
		/* Call action handler */
		return ADI_COMMON_ACT_ERR_RESET_FULL;
	}

	/* Set tx LO frequency to 500MHz */
	recoveryAction = adi_adrv9029_PllFreqSet(ADI_ADRV9025_LO2_PLL, 500000000);
	if (recoveryAction != ADI_COMMON_ACT_NO_ACTION)
	{
		printf("Failed to enable ADRV9025_B Rx RF input\n");
		/* Call action handler */
		return ADI_COMMON_ACT_ERR_RESET_FULL;
	}

	/************************************************************************
	 *                    END: Initialize ADRV9029 RX                        *
	 ************************************************************************/

	Status = axiDma_init();
	if (Status != XST_SUCCESS)
	{
		xil_printf("Failed intr setup\r\n");
		return XST_FAILURE;
	}

	/* Initialize tcp timer */
	Status = setup_tcp_timer();
	if (Status != XST_SUCCESS)
	{
		xil_printf("Failed intr setup\r\n");
		return XST_FAILURE;
	}

	/* Setup system interrupt */
	Status = setup_intr_system(&intc);
	if (Status != XST_SUCCESS)
	{
		xil_printf("Failed intr setup\r\n");
		return XST_FAILURE;
	}

	print_app_header();

	start_tcp_app();

	int timeout;
	int Remain_Len;
	int transLen;
	u8 *tcp_send_ptr;
	raw_data_head_t d_head;

	d_head.PREAMBLE = 0x55555555;

	printf("end\n");

//		while (1)
//		{
//			set_Aaf_Coe_Preset(0);
//			set_Aaf_Coe_Preset(1);
//			set_Aaf_Coe_Preset(2);
//			set_Aaf_Coe_Preset(3);
//			set_Aaf_Coe_Preset(4);
//			set_Aaf_Coe_Preset(5);
//			set_Aaf_Coe_Preset(6);
//		}

//    while (1) {
//        PSDK_ParsedData_t parsed_data = parsePSDKData();
//        printPSDKData(&parsed_data);
//        usleep(1000000); // 1 second
//    }

//	    while (1) {
//	    	self_test_info_t retVal;
//	        /* Get RFCH Status */
//	        retVal.RFCH_ERR_CODE = RFCH_RD_CFG_CTRL_RREG(0);
//	        retVal.RFCH_ERR_CH_MASK = RFCH_RD_CFG_CTRL_RREG(4);
//	        retVal.RFCH_TEMPETURE = RFCH_RD_CFG_CTRL_RREG(12);
//	        retVal.RFCH_ID = RFCH_RD_CFG_CTRL_RREG(36);
//	    }

	while (1)
	{
		if (TcpFastTmrFlag)
		{
			tcp_fasttmr();
			TcpFastTmrFlag = 0;
		}
		if (TcpSlowTmrFlag)
		{
			tcp_slowtmr();
			TcpSlowTmrFlag = 0;
		}
		xemacif_input(&server_netif);

		if (tcp_client_connected)
		{
			switch (op_cmd)
			{
			case OP_CMD_NONE:
				timeout = DMA_TIMEOUT_COUNTER;
				//sleep(1000);//1ms
				if(flag==3)
				{
				self_test_info_t SelfTestInfo;
				SelfTestInfo = getSelfTestInfo();
				tcp_send_data(&SelfTestInfo, sizeof(self_test_info_t));
printf("self_test_info_t length=%d",sizeof(self_test_info_t));
				d_head.IMU_DATA = getImuData();//注意改成PSDK的
				Status = tcp_send_data(&d_head.IMU_DATA, sizeof(d_head.IMU_DATA));
									if (Status != XST_SUCCESS)
										break;
				flag=0;
				}



				break;

			case OP_CMD_TRANS_START:
				if (!dma_all_done)
				{
					timeout--;
					usleep(10);
				}
				else
				{
					tcp_send_ptr = (u8 *)TCP_BUFFER_BASE;
				if(flag==1)
				{
					if (samp_cfg.chs == 0)
						Remain_Len = samp_cfg.swept_num * samp_cfg.samp_num * BYTES_PER_SAMPLE;
					//	Remain_Len = samp_cfg_collect_data.swept_num * samp_cfg_collect_data.samp_num * BYTES_PER_SAMPLE;
					else
						Remain_Len = samp_cfg.swept_num * samp_cfg.samp_num * BYTES_PER_SAMPLE / 8;
					//	Remain_Len = samp_cfg_collect_data.swept_num * samp_cfg_collect_data.samp_num * BYTES_PER_SAMPLE / 8;
					d_head.SAMP_CFG = samp_cfg;
				}
				else if(flag==2)
				{
					if (samp_cfg_collect_data.chs == 0)
						Remain_Len = samp_cfg_collect_data.swept_num * samp_cfg_collect_data.samp_num * BYTES_PER_SAMPLE;
					else
						Remain_Len = samp_cfg_collect_data.swept_num * samp_cfg_collect_data.samp_num * BYTES_PER_SAMPLE / 8;
					d_head.SAMP_CFG = samp_cfg_collect_data;
				}
					//d_head.SAMP_CFG = samp_cfg_collect_data;
					d_head.IMU_DATA = getImuData();//注意改成PSDK的
					d_head.LO_FREQ = op_cfg.lo_freq;
					//printf("lofreq0 %d \n", op_cfg.lo_freq/2);
					d_head.FILTER_SEL = op_cfg.filter_sel;
					d_head.EXT_NUM = op_cfg.extract_num;
					d_head.RX_GAIN = op_cfg.rx_gain;

					d_head.CAL_PHA = getcal_phase();

					d_head.SAMP_CFG.reserve2 = lo_lna_sw;

					Status = tcp_send_data(&d_head, sizeof(d_head));
					if (Status != XST_SUCCESS)
						break;
//lofreq0
					op_cmd = OP_CMD_TRANSING;
				}
				if (timeout == 0)
				{
					dma_all_done = 0;
					op_cmd = OP_CMD_NONE;
					xil_printf("axi dma timeout!\r\n");
					return XST_FAILURE;
				}
				break;

			case OP_CMD_TRANSING:
				/* Send max to <TCP_MAX_SEND_LEN> bytes each tcp transfer */
				//固定remain_len
				//固定搬运同一段ddr数据给tcp，实现连续发送

			//	Remain_Len = samp_cfg.swept_num * samp_cfg.samp_num * BYTES_PER_SAMPLE;
			//	tcp_send_ptr = (u8 *)TCP_BUFFER_BASE;
			//	memcpy((UINTPTR)TCP_BUFFER_BASE, (UINTPTR)RX_BUFFER_BASE, 65536 * 32*134);
				//*********************************//
				if (Remain_Len > 0)
				{
					transLen = (Remain_Len > TCP_MAX_SEND_LEN) ? TCP_MAX_SEND_LEN : Remain_Len;
					Status = tcp_send_data(tcp_send_ptr, transLen);
					if (Status != XST_SUCCESS)
						break;
					tcp_send_ptr += transLen;
					Remain_Len -= transLen;
				}
				else
				{
					dma_all_done = 0;
					axiDma_reset();
					op_cmd = OP_CMD_NONE;
					//op_cmd=OP_CMD_TRANSING;
				}
				break;

			default:
				break;
			}
		}
	}
//b	_boot
	while (1)
		;
	cleanup_platform();
	return 0;
}
