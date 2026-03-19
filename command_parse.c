#include "command_parse.h"
#include "hy_types.h"
#include "perip_ctrl.h"
#include "gpio_emio.h"
#include "xil_cache.h"
#include "sleep.h"
#include "dma_utilities.h"
#include "adi_utilities.h"
#include "tcp_utilities.h"
#include "ch_calib.h"

int op_cmd = OP_CMD_NONE;
samp_cfg_t samp_cfg;
samp_cfg_t samp_cfg_collect_data;
op_config_t op_cfg;
uint8_t lo_lna_sw;
uint8_t state;
uint8_t ana_filter_sel;

int flag=0;
int dds_or_real=0;
int number_dds=0;
extern volatile int dma_rx_done;
extern volatile int dma_all_done;
void tcp_command_parse(u8 *cmd, int cmd_len)
{
	if (cmd[0] == 0xd5)
	{
		//set_Aaf_DDS_or_Real(dds_or_real);
		set_Aaf_DDS_or_Real(0);
		//set_Aaf_DDS_eight_phase();
		flag=1;
		op_cmd = OP_CMD_TRANS_START;
		memcpy(&samp_cfg, &cmd[1], sizeof(samp_cfg_t));
		if (samp_cfg.samp_num > 65536)
			samp_cfg.samp_num = 65536;
		dma_rx_done = 0;
		dma_all_done = 0;
		xil_printf("Start sampling! Point: %d\r\n", samp_cfg.samp_num);
//		state = samp_cfg.state;
		start_dma_trans(&samp_cfg);
	}

	else if (cmd[0] == 0xc4)
	{
		memcpy(&op_cfg, &cmd[1], sizeof(op_config_t));

		set_Extract_Num(op_cfg.extract_num);

		set_CalSw_Period(op_cfg.sw_period);
		state = op_cfg.state;
		xil_printf("State: %d\r\n", state);

//		if (op_cfg.lna_sw == LNA_SW_AUTO) // Auto
//		{
//			if (op_cfg.lo_freq > 12000)
//				lo_lna_sw = LNA_SW_HF;
//			else
//				lo_lna_sw = LNA_SW_LF;
//			setLnaSw(lo_lna_sw);
//		}
//		else
//		{
//			lo_lna_sw = op_cfg.lna_sw;
//			setLnaSw(op_cfg.lna_sw);
//		}
		ana_filter_sel = 0;
		if (op_cfg.filter_sel <= 4)
		{
			set_Aaf_Coe_Preset(1);
			ana_filter_sel = 0;
		}
		//0 40
		//1 20
		//2 10
		//3 5
		//4 2
		//5 1
		//6 650k

				else if (op_cfg.filter_sel <= 6)
		{
			set_Aaf_Coe_Preset(op_cfg.filter_sel);
			ana_filter_sel = 1;
		}
		else
		{
			set_Aaf_Coe_Preset(6);
			ana_filter_sel = 1;
		}

		if (op_cfg.lna_sw == LNA_SW_CAL)
		{
		    setRfChPara(0xFF, 1, op_cfg.lo_freq, ana_filter_sel, op_cfg.rx_gain);//重新配置回初始的采样频率
		    if (state == 1){
		    usleep(2000000);}
		}
		else
		{
		    setRfChPara(0x7F, 1, op_cfg.lo_freq, ana_filter_sel, op_cfg.rx_gain);//重新配置回初始的采样频率
		}
		usleep(10000);
		RfChCfgTrig();

		printf("\n-----------------------------------------------\n");
		printf("           Sampling Config Changed!\n");
		printf("-----------------------------------------------\n");
		printf("    lo_freq       :    %d MHz\n", op_cfg.lo_freq / 2);
		printf("    filter_sel    :    %d\n", op_cfg.filter_sel);
		printf("    rx_gain       :    %d dB\n", op_cfg.rx_gain - 20);
		printf("    tx_gain       :    %d dB\n", op_cfg.tx_gain - 20);
		printf("    lna_sw        :    %d\n", op_cfg.lna_sw);
		printf("    samp_freq     :    %.3f MHz\n", (double)100 / op_cfg.extract_num);
		printf("    sw_period     :    %d\n", op_cfg.sw_period);
		printf("-----------------------------------------------\n");
		op_cmd = OP_CMD_NONE;
	}
	else if (cmd[0] == 0xdf)
	{
		flag=2;
		op_cmd = OP_CMD_TRANS_START;
		memcpy(&samp_cfg_collect_data, &cmd[1], sizeof(samp_cfg_t));
		if (samp_cfg_collect_data.samp_num > 65536)
			samp_cfg_collect_data.samp_num = 65536;
		dma_rx_done = 0;
		dma_all_done = 0;
		xil_printf("Start sampling! Point: %d\r\n", samp_cfg_collect_data.samp_num);
//		state = samp_cfg.state;
		start_dma_trans_collect_data(&samp_cfg_collect_data);
	}
	else if (cmd[0] == 0x1e)
	{
		if (cmd[1] == 0x01)
		{
			flag=3;
			number_dds++;
			if(number_dds%2==0)
			dds_or_real=0;
			else if(number_dds%2==1)
			{
				dds_or_real=1;
			}
			//if(number_dds>=5)
			//{
			//	set_Aaf_DDS_eight_phase(0x0E390000,0x2AAB2000,0x55554000,0x4000644B);

			//	xil_printf("eight_phase: %d\r\n", 1);
			//}
			//	xil_printf("dds_or_real: %d\r\n", dds_or_real);
			//	xil_printf("number_dds: %d\r\n", number_dds);
			//dds_or_real=1;
			printf("SUCCESS HEART\n");

		}
	}
}

int start_dma_trans(samp_cfg_t * sampCfg)
{
    int Status;
    int timeout;
    int Length;

    Length = samp_cfg.samp_num * BYTES_PER_SAMPLE;//一次采样字节数

    set_DmaFifo_WrLen(Length);

    if(sampCfg->swept_num > 1){
		if (op_cfg.lna_sw == LNA_SW_CAL)
		{
			setRfChPara(0xFF, 2, sampCfg->swept_step, ana_filter_sel, op_cfg.rx_gain);//重新配置回初始的采样频率
//		    if (state == 1){
//		    sleep(1);}
		}
		else
		{
			setRfChPara(0x7F, 2, sampCfg->swept_step, ana_filter_sel, op_cfg.rx_gain);//重新配置回初始的采样频率
		}
    }

    usleep(10000);

    for (int i = 0; i < sampCfg->swept_num; i++)
    {
        /* Start DMA transfer */
        Status = axiDma_transfer_dev2dma((UINTPTR)(RX_BUFFER_BASE+i*Length), Length);
        if (Status != XST_SUCCESS)
        {
            xil_printf("axi dma failed! Status: %d\r\n", Status);
            return Status;
        }

        fifo_Wr_Trig();//wr_en

        timeout = DMA_TIMEOUT_COUNTER;

        while (1)
        {
        	if (dma_rx_done)
        		break;
        	else
        	{
        		timeout--;
        		usleep(1);

        		if (timeout == 0)
        		{
        			xil_printf("axi dma timeout! Status: %d\r\n", Status);
        			return XST_FAILURE;
        		}
        	}
        }

      // 	volatile int32_t *buf = RX_BUFFER_BASE;
      //  printf("buf[0] = %d\n", buf[0]);
      //  buf[0] = 1;
      //  printf("after write1 buf[0] = %d\n", buf[0]);

		Xil_DCacheFlushRange((UINTPTR)(RX_BUFFER_BASE+i*Length), Length);

		RfChCfgTrig();
		dma_rx_done = 0;

		// printf("after write2 buf[0] = %d\n", buf[0]);



		if(samp_cfg.sig_use == 0x02){//			通道校正
		uint8_t ana_filter_sel;
		if (op_cfg.filter_sel <= 4)
		{
			ana_filter_sel = 0;
		}
		else
		{
			ana_filter_sel = 1;
		}
			perform_phase_and_amplitude_correction(samp_cfg.samp_num, op_cfg.lo_freq, 0, ana_filter_sel);//数据还是trig之前的
		}

//		if(samp_cfg.Direction_Mode == 0x01){
//			int channel_idx = 0;  // 第1个通道
//			int has_signal = run_signal_detection(i*Length, channel_idx, samp_cfg.samp_num);
//			if (has_signal)
//			    printf("通道 %d 检测到信号\r\n", channel_idx);
//			else
//			    printf("通道 %d 无信号\r\n", channel_idx);
//    }

//		if ((op_cfg.lo_freq+(i+1)*sampCfg->swept_step > 12000) && (op_cfg.lna_sw == LNA_SW_AUTO) && (lo_lna_sw == LNA_SW_LF))//判断跳完需不需要切换开关，2~6一个，6~18一个
//		{
//			lo_lna_sw = LNA_SW_HF;
//			setLnaSw(lo_lna_sw);
//		}

		usleep(1000);
    }

    dma_all_done = 1;

    if (sampCfg->chs == 0)
    {
    //	IQSample buffer1[samp_cfg.samp_num * NUM_CHANNELS];
       // volatile int32_t *buf = RX_BUFFER_BASE;
       //printf("buf[0] = %d\n", buf[0]);
     //  +
    //	(0,0, samp_cfg.samp_num, op_cfg.lo_freq/2);
    	//int total = samp_cfg.samp_num * NUM_CHANNELS * sizeof(IQSample);
    	//Xil_DCacheFlushRange((UINTPTR)(RX_BUFFER_BASE+total), total);
    	//printf("after write buf[0] = %d\n", buf[0]);
    	//memcpy(buffer1, (void *)RX_BUFFER_BASE, samp_cfg.samp_num * sizeof(IQSample) * NUM_CHANNELS);
    	//printf("buf[10] = %d\n", buffer[10]);
    	//buffer[10]=200;
    	//memcpy((UINTPTR)RX_BUFFER_BASE, buffer, samp_num * sizeof(IQSample) * NUM_CHANNELS);
    	memcpy((UINTPTR)TCP_BUFFER_BASE, (UINTPTR)RX_BUFFER_BASE, Length*sampCfg->swept_num);
    }
    else
    {
        for (int i = 0; i < ((Length*sampCfg->swept_num/8)/4); i++)//在从一个 8 通道交织的数据缓冲区中，提取某个特定通道的采样点总字节数(Length*sampCfg->swept_num/8)应该/4
        {
        	memcpy((UINTPTR)TCP_BUFFER_BASE+i*sizeof(int16_t)*2, (UINTPTR)RX_BUFFER_BASE+(sampCfg->chs-1+i*8)*sizeof(int16_t)*2 , sizeof(int16_t)*2);//一次搬4个字节
        }
    }

	if (op_cfg.lna_sw == LNA_SW_CAL)
	{
	    setRfChPara(0xFF, 1, op_cfg.lo_freq, ana_filter_sel, op_cfg.rx_gain);//重新配置回初始的采样频率
//	    if (state == 1){
//	    sleep(1);}
	}
	else
	{
	    setRfChPara(0x7F, 1, op_cfg.lo_freq, ana_filter_sel, op_cfg.rx_gain);//重新配置回初始的采样频率
	}
	usleep(10000);
	RfChCfgTrig();

    return XST_SUCCESS;
}

int start_dma_trans_collect_data(samp_cfg_t * sampCfg)
{
    int Status;
    int timeout;
    int Length;

    Length = samp_cfg_collect_data.samp_num * BYTES_PER_SAMPLE;//一次采样字节数

    set_DmaFifo_WrLen(Length);

    if(sampCfg->swept_num > 1){
		if (op_cfg.lna_sw == LNA_SW_CAL)
		{
			setRfChPara(0xFF, 2, sampCfg->swept_step, ana_filter_sel, op_cfg.rx_gain);//重新配置回初始的采样频率
//		    if (state == 1){
//		    sleep(1);}
		}
		else
		{
			setRfChPara(0x7F, 2, sampCfg->swept_step, ana_filter_sel, op_cfg.rx_gain);//重新配置回初始的采样频率
		}
    }

    usleep(10000);

    for (int i = 0; i < sampCfg->swept_num; i++)
    {
        /* Start DMA transfer */
        Status = axiDma_transfer_dev2dma((UINTPTR)(RX_BUFFER_BASE+i*Length), Length);
        if (Status != XST_SUCCESS)
        {
            xil_printf("axi dma failed! Status: %d\r\n", Status);
            return Status;
        }

        fifo_Wr_Trig();//wr_en

        timeout = DMA_TIMEOUT_COUNTER;

        while (1)
        {
        	if (dma_rx_done)
        		break;
        	else
        	{
        		timeout--;
        		usleep(1);

        		if (timeout == 0)
        		{
        			xil_printf("axi dma timeout! Status: %d\r\n", Status);
        			return XST_FAILURE;
        		}
        	}
        }

		Xil_DCacheFlushRange((UINTPTR)(RX_BUFFER_BASE+i*Length), Length);
//取消采集频点递增
		//RfChCfgTrig();
		dma_rx_done = 0;

//		if(samp_cfg.calib_calculate == 0x01){//			通道校正
//		uint8_t ana_filter_sel;
//		if (op_cfg.filter_sel <= 4)
//		{
//			ana_filter_sel = 0;
//		}
//		else
//		{
//			ana_filter_sel = 1;
//		}
//			perform_phase_and_amplitude_correction(samp_cfg.samp_num, op_cfg.lo_freq+i*sampCfg->swept_step, i*Length, ana_filter_sel);//数据还是trig之前的
//				}

//		if(samp_cfg.Direction_Mode == 0x01){
//			int channel_idx = 0;  // 第1个通道
//			int has_signal = run_signal_detection(i*Length, channel_idx, samp_cfg.samp_num);
//			if (has_signal)
//			    printf("通道 %d 检测到信号\r\n", channel_idx);
//			else
//			    printf("通道 %d 无信号\r\n", channel_idx);
//    }

//		if ((op_cfg.lo_freq+(i+1)*sampCfg->swept_step > 12000) && (op_cfg.lna_sw == LNA_SW_AUTO) && (lo_lna_sw == LNA_SW_LF))//判断跳完需不需要切换开关，2~6一个，6~18一个
//		{
//			lo_lna_sw = LNA_SW_HF;
//			setLnaSw(lo_lna_sw);
//		}
		usleep(50000);
    }

    dma_all_done = 1;

    if (samp_cfg_collect_data.chs == 0)
    {
    	memcpy((UINTPTR)TCP_BUFFER_BASE, (UINTPTR)RX_BUFFER_BASE, Length*samp_cfg_collect_data.swept_num);
    }
    else
    {
        for (int i = 0; i < ((Length*sampCfg->swept_num/8)/4); i++)//在从一个 8 通道交织的数据缓冲区中，提取某个特定通道的采样点总字节数(Length*sampCfg->swept_num/8)应该/4
        {
        memcpy((UINTPTR)TCP_BUFFER_BASE+i*sizeof(int16_t)*2, (UINTPTR)RX_BUFFER_BASE+(sampCfg->chs-1+i*8)*sizeof(int16_t)*2 , sizeof(int16_t)*2);//一次搬4个字节
        }
    }

	if (op_cfg.lna_sw == LNA_SW_CAL)
	{
	    setRfChPara(0xFF, 1, op_cfg.lo_freq, ana_filter_sel, op_cfg.rx_gain);//重新配置回初始的采样频率
//	    if (state == 1){
//	    sleep(1);}
	}
	else
	{
	    setRfChPara(0x7F, 1, op_cfg.lo_freq, ana_filter_sel, op_cfg.rx_gain);//重新配置回初始的采样频率
	}
	//usleep(10000);
	usleep(10000);
	//
	RfChCfgTrig();

    return XST_SUCCESS;
}



