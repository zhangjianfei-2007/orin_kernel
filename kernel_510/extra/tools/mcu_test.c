/*
 * SPI testing utility (using spidev driver)
 *
 * Copyright (c) 2007  MontaVista Software, Inc.
 * Copyright (c) 2007  Anton Vorontsov <avorontsov@ru.mvista.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License.
 *
 * Cross-compile with cross-gcc -I/path/to/cross-kernel/include
 */

#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/ioctl.h>
#include <sys/stat.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>



#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

#define MAX_BUF 70
#define SPIFRAMELEN 1162
#define SPIHEADER1 0x55
#define SPIHEADER2 0xAA
/* 设置打印字体颜色、粗细 */
#define NONE                 "\e[0m"
#define BLACK                "\e[0;30m"
#define L_BLACK              "\e[1;30m"
#define RED                  "\e[0;31m"
#define L_RED                "\e[1;31m"
#define GREEN                "\e[0;32m"
#define L_GREEN              "\e[1;32m"
#define BROWN                "\e[0;33m"
#define YELLOW               "\e[1;33m"
#define BLUE                 "\e[0;34m"
#define L_BLUE               "\e[1;34m"
#define PURPLE               "\e[0;35m"
#define L_PURPLE             "\e[1;35m"
#define CYAN                 "\e[0;36m"
#define L_CYAN               "\e[1;36m"
#define GRAY                 "\e[0;37m"
#define WHITE                "\e[1;37m"

#define BOLD                 "\e[1m"
#define UNDERLINE            "\e[4m"
#define BLINK                "\e[5m"
#define REVERSE              "\e[7m"
#define HIDE                 "\e[8m"
#define CLEAR                "\e[2J"
#define CLRLINE              "\r\e[K" //or "\e[1K\r"

static const char *device = "/dev/spidev1.0";
static uint8_t bits = 8;
static uint32_t speed = 10000000;
unsigned int SPI_miss_count = 0;
unsigned int SPI_checksum_error_count = 0;
unsigned int SPI_Other_error_count = 0;

/* 通道号枚举值 */
typedef enum
{
	MCU_CHN_A = 0,
	MCU_CHN_B = 1,
	MCU_CHN_C = 2,
} MCUChannel;

/* CAN帧格式结构体定义 */
typedef struct
{
	unsigned char channel;
	unsigned char id[4];
	unsigned char len;
	unsigned char data[8];
} CANFrame;

/* CAN帧类型定义，标准帧 */
typedef enum
{
	CANFRAME = 1,
} FrameType;

/* SPI设备枚举值 */
typedef enum
{
	CONTROLLER1 = 1,	// 主XAVIER1
	CONTROLLER2 = 2,	// 主XAVIER2
	MCU = 3,			// MCU
} SPIDevice;

/* SPI帧格式结构体定义 */
typedef struct
{
	unsigned char header[2];
	unsigned char frame_len[2];
	unsigned char frame_type;
	unsigned char data_len;
	unsigned char src;
	unsigned char dest;
	CANFrame can_frame_buf[MAX_BUF];
	unsigned char reserve[10];
	unsigned char checksum;
	unsigned char rolling_count;
} SPIFrame;

/* SPI帧解析中的错误码枚举值 */
typedef enum
{
	SPIFRAME_START = 0,
	SPIFRAME_HEADER1_CHECK_ERROR = 101,
	SPIFRAME_HEADER2_CHECK_ERROR = 102,
	SPIFRAME_FRAME_LEN_CHECK_ERROR = 103,
	SPIFRAME_FRAME_TYPE_CHECK_ERROR = 104,
	SPIFRAME_DATA_LEN_CHECK_ERROR = 105,
	SPIFRAME_SRC_CHECK_ERROR = 106,
	SPIFRAME_DEST_CHECK_ERROR = 107,
	SPIFRAME_ROLLING_COUNT_CHECK_ERROR = 108,
	SPIFRAME_CHECKSUM_CHECK_ERROR = 109,
	SPIFRAME_PARSE_MISS = 201,
	CANFRAME_PARSE_MISS = 202,
	SPI_PARSE_COMPLETE = 250,
} MCUErrorCode;

/* SPI帧的发送、接收变量 */
SPIFrame spi_frame_tx_;
SPIFrame spi_frame_rx_;

/* SPI帧的帧头值定义 */
const unsigned char header1 = 0x55;
const unsigned char header2 = 0xAA;

/*
 执行硬件上的SPI数据传输 
 fd:SPI设备文件句柄
 tx:发送缓冲器指针
 rx:接收缓冲区指针
 len:收发的数据个数
*/
static int transfer(int fd, uint8_t const *tx, uint8_t const *rx, size_t len)
{
	int ret = 0;
	struct spi_ioc_transfer tr;
	/* 设置SPI传输的控制结构体 */
	memset(&tr, 0, sizeof(struct spi_ioc_transfer));
	tr.tx_buf = (unsigned long)tx;	// 发送缓冲区
	tr.rx_buf = (unsigned long)rx;	// 接收缓冲区
	tr.len = len;					// 收发数据个数
	tr.delay_usecs = 0;				// SPI周期间的延时
	tr.speed_hz = speed;			// SPI时钟频率
	tr.bits_per_word = bits;		// SPI周期中的字节的bit数
	/* 执行SPI传输 */
	ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
	usleep(3000);
	if (ret < 1)
	{
		printf("\ncan't send spi message\r\n");
		return (ret = 1);
	}
	return (ret = 0);
}

/*
 计算checksum值 
 data:待计算的数据指针
 len:待计算的数据个数
*/
unsigned char CheckSum(unsigned char *data, unsigned short len)
{
	unsigned char check = 0;
	for (unsigned short i = 0; i <= len - 1; i++)
	{
		check += data[i];
	}
	return check;
}

/*
 解析并校验接收到的SPI帧数据格式
 spi_frame:待解析的SPI帧数据指针
*/
unsigned char SPIFrameParse(SPIFrame *spi_frame)
{
	unsigned int cnt_diff = 0;
	unsigned int error_code = 0;
	unsigned short frame_len = 0;
	static unsigned char last_rolling_cnt = 1;
	unsigned char check = 0;

	/* SPI帧头1错误 */
	if (spi_frame->header[0] != header1)
	{
		printf(L_RED "SPIFRAME_HEADER1_CHECK_ERROR\r\n" NONE);
		printf("head1=%2x", spi_frame->header[0]);
		return (unsigned char)SPIFRAME_HEADER1_CHECK_ERROR;
	}
	/* SPI帧头2错误 */
	if (spi_frame->header[1] != header2)
	{
		printf(L_RED "SPIFRAME_HEADER2_CHECK_ERROR\r\n" NONE);
		return (unsigned char)SPIFRAME_HEADER2_CHECK_ERROR;
	}

	frame_len = (unsigned short)spi_frame->frame_len[0] |
				(unsigned short)spi_frame->frame_len[1] << 8;
	/* SPI帧长度错误 */
	if (frame_len != sizeof(SPIFrame))
	{
		printf(L_RED "SPIFRAME_FRAME_LEN_CHECK_ERROR\r\n" NONE);
		return (unsigned char)SPIFRAME_FRAME_LEN_CHECK_ERROR;
	}
	/* SPI帧类型错误 */
	switch ((FrameType)spi_frame->frame_type)
	{
	case CANFRAME:
		break;
	default:
		printf(L_RED "SPIFRAME_FRAME_TYPE_CHECK_ERROR\r\n" NONE);
		return (unsigned char)SPIFRAME_FRAME_TYPE_CHECK_ERROR;
	}
	/* SPI帧中CAN数据超出最大允许个数错误 */
	if (spi_frame->data_len > MAX_BUF)
	{
		printf(L_RED "SPIFRAME_DATA_LEN_CHECK_ERROR\r\n" NONE);
		return (unsigned char)SPIFRAME_DATA_LEN_CHECK_ERROR;
	}
	/* SPI帧数据的源设备错误 */
	switch ((SPIDevice)spi_frame->src)
	{
	case MCU:
		break;
	default:
		printf(L_RED "SPIFRAME_SRC_CHECK_ERROR\r\n" NONE);
		return (unsigned char)SPIFRAME_SRC_CHECK_ERROR;
	}
	/* SPI帧数据的目标设备错误 */
	switch ((SPIDevice)spi_frame->dest)
	{
	case CONTROLLER1:
	case CONTROLLER2:
		break;
	default:
		printf(L_RED "SPIFRAME_DEST_CHECK_ERROR\r\n" NONE);
		return (unsigned char)SPIFRAME_DEST_CHECK_ERROR;
	}

	check = CheckSum((unsigned char *)spi_frame, sizeof(SPIFrame) - 2);
	/* SPI帧数据的checksum值错误 */
	if (check != spi_frame->checksum)
	{
		printf(L_RED "SPIFRAME_CHECKSUM_CHECK_ERROR\r\n" NONE);
		return (unsigned char)SPIFRAME_CHECKSUM_CHECK_ERROR;
	}

	// if(spi_frame->rolling_count < last_rolling_cnt) {
	// 	cnt_diff = spi_frame->rolling_count + 256 - last_rolling_cnt;
	// } else {
	// 	cnt_diff = spi_frame->rolling_count - last_rolling_cnt;
	// }
	// if(cnt_diff != 1 && cnt_diff) {
	// 	printf(L_RED "SPIFRAME_PARSE_MISS\r\n" NONE);
	// 	return (unsigned char)SPIFRAME_PARSE_MISS;
	// }
	// std::cout<<"err:"<<(int)error_code<<std::endl;
	return error_code;
}

/*
 初始化要发送出去的SPI帧数据内容
*/
void initSendFrame(void)
{
	memset(spi_frame_tx_.can_frame_buf, 0, sizeof(spi_frame_tx_.can_frame_buf));
	
	spi_frame_tx_.header[0] = 0x55;
	spi_frame_tx_.header[1] = 0xAA;
	spi_frame_tx_.frame_len[0] = 0xE8;
	spi_frame_tx_.frame_len[1] = 0x03;
	spi_frame_tx_.frame_type = CANFRAME;
	spi_frame_tx_.src = CONTROLLER1;
	spi_frame_tx_.dest = MCU;
	/* 设置要发送的CAN帧数据个数和内容 */
	spi_frame_tx_.data_len = 6;
	spi_frame_tx_.can_frame_buf[0].channel = 0;
	spi_frame_tx_.can_frame_buf[0].id[0] = 0xA1;
	spi_frame_tx_.can_frame_buf[0].id[1] = 0x00;
	spi_frame_tx_.can_frame_buf[0].id[2] = 0x00;
	spi_frame_tx_.can_frame_buf[0].id[3] = 0x00;
	spi_frame_tx_.can_frame_buf[0].len = 8;
	spi_frame_tx_.can_frame_buf[0].data[0] = 2;
	spi_frame_tx_.can_frame_buf[0].data[1] = 3;
	spi_frame_tx_.can_frame_buf[0].data[2] = 4;
	spi_frame_tx_.can_frame_buf[0].data[3] = 5;
	spi_frame_tx_.can_frame_buf[0].data[4] = 6;
	spi_frame_tx_.can_frame_buf[0].data[5] = 7;
	spi_frame_tx_.can_frame_buf[0].data[6] = 8;
	spi_frame_tx_.can_frame_buf[0].data[7] = 9; 

	spi_frame_tx_.can_frame_buf[1].channel = 0;
	spi_frame_tx_.can_frame_buf[1].id[0] = 0xA2;
	spi_frame_tx_.can_frame_buf[1].id[1] = 0x00;
	spi_frame_tx_.can_frame_buf[1].id[2] = 0x00;
	spi_frame_tx_.can_frame_buf[1].id[3] = 0x00;
	spi_frame_tx_.can_frame_buf[1].len = 8;
	spi_frame_tx_.can_frame_buf[1].data[0] = 22;
	spi_frame_tx_.can_frame_buf[1].data[1] = 33;
	spi_frame_tx_.can_frame_buf[1].data[2] = 44;
	spi_frame_tx_.can_frame_buf[1].data[3] = 55;
	spi_frame_tx_.can_frame_buf[1].data[4] = 66;
	spi_frame_tx_.can_frame_buf[1].data[5] = 77;
	spi_frame_tx_.can_frame_buf[1].data[6] = 88;
	spi_frame_tx_.can_frame_buf[1].data[7] = 99;

	spi_frame_tx_.can_frame_buf[2].channel = 1;
	spi_frame_tx_.can_frame_buf[2].id[0] = 0xB1;
	spi_frame_tx_.can_frame_buf[2].id[1] = 0x00;
	spi_frame_tx_.can_frame_buf[2].id[2] = 0x00;
	spi_frame_tx_.can_frame_buf[2].id[3] = 0x00;
	spi_frame_tx_.can_frame_buf[2].len = 8;
	spi_frame_tx_.can_frame_buf[2].data[0] = 2;
	spi_frame_tx_.can_frame_buf[2].data[1] = 3;
	spi_frame_tx_.can_frame_buf[2].data[2] = 4;
	spi_frame_tx_.can_frame_buf[2].data[3] = 5;
	spi_frame_tx_.can_frame_buf[2].data[4] = 6;
	spi_frame_tx_.can_frame_buf[2].data[5] = 7;
	spi_frame_tx_.can_frame_buf[2].data[6] = 8;
	spi_frame_tx_.can_frame_buf[2].data[7] = 9; 

	spi_frame_tx_.can_frame_buf[3].channel = 1;
	spi_frame_tx_.can_frame_buf[3].id[0] = 0xB2;
	spi_frame_tx_.can_frame_buf[3].id[1] = 0x00;
	spi_frame_tx_.can_frame_buf[3].id[2] = 0x00;
	spi_frame_tx_.can_frame_buf[3].id[3] = 0x00;
	spi_frame_tx_.can_frame_buf[3].len = 8;
	spi_frame_tx_.can_frame_buf[3].data[0] = 22;
	spi_frame_tx_.can_frame_buf[3].data[1] = 33;
	spi_frame_tx_.can_frame_buf[3].data[2] = 44;
	spi_frame_tx_.can_frame_buf[3].data[3] = 55;
	spi_frame_tx_.can_frame_buf[3].data[4] = 66;
	spi_frame_tx_.can_frame_buf[3].data[5] = 77;
	spi_frame_tx_.can_frame_buf[3].data[6] = 88;
	spi_frame_tx_.can_frame_buf[3].data[7] = 99;

	spi_frame_tx_.can_frame_buf[4].channel = 2;
	spi_frame_tx_.can_frame_buf[4].id[0] = 0xC1;
	spi_frame_tx_.can_frame_buf[4].id[1] = 0x00;
	spi_frame_tx_.can_frame_buf[4].id[2] = 0x00;
	spi_frame_tx_.can_frame_buf[4].id[3] = 0x00;
	spi_frame_tx_.can_frame_buf[4].len = 8;
	spi_frame_tx_.can_frame_buf[4].data[0] = 2;
	spi_frame_tx_.can_frame_buf[4].data[1] = 3;
	spi_frame_tx_.can_frame_buf[4].data[2] = 4;
	spi_frame_tx_.can_frame_buf[4].data[3] = 5;
	spi_frame_tx_.can_frame_buf[4].data[4] = 6;
	spi_frame_tx_.can_frame_buf[4].data[5] = 7;
	spi_frame_tx_.can_frame_buf[4].data[6] = 8;
	spi_frame_tx_.can_frame_buf[4].data[7] = 9; 

	spi_frame_tx_.can_frame_buf[5].channel = 2;
	spi_frame_tx_.can_frame_buf[5].id[0] = 0xC2;
	spi_frame_tx_.can_frame_buf[5].id[1] = 0x00;
	spi_frame_tx_.can_frame_buf[5].id[2] = 0x00;
	spi_frame_tx_.can_frame_buf[5].id[3] = 0x00;
	spi_frame_tx_.can_frame_buf[5].len = 8;
	spi_frame_tx_.can_frame_buf[5].data[0] = 22;
	spi_frame_tx_.can_frame_buf[5].data[1] = 33;
	spi_frame_tx_.can_frame_buf[5].data[2] = 44;
	spi_frame_tx_.can_frame_buf[5].data[3] = 55;
	spi_frame_tx_.can_frame_buf[5].data[4] = 66;
	spi_frame_tx_.can_frame_buf[5].data[5] = 77;
	spi_frame_tx_.can_frame_buf[5].data[6] = 88;
	spi_frame_tx_.can_frame_buf[5].data[7] = 99;
	spi_frame_tx_.checksum = CheckSum((unsigned char *)&spi_frame_tx_, sizeof(SPIFrame) - 2);
}


#define MCU_VERSION_REQ_ID		(839)	// 版本号请求CAN ID
#define MCU_STATUS_REQ_ID		(838)	// 状态请求CAN ID
#define MCU_ERROR_REQ_ID		(837)	// 错误请求CAN ID

int main(int argc, char *argv[])
{
	int ret = 0;
	int fd;
	unsigned char data = 0;
	int date_byte = 0;
	unsigned char rolling_cnt_ = 1;
	unsigned char last_recv_rolling_cnt = 0;
	uint32_t recv_id = 0;
	uint32_t recv_target_id_cnt = 0;
	uint32_t send_src_id_cnt = 0;
	int parse_ret = 0;
	unsigned char recvCanFrameCnt = 0;
	unsigned char i = 0, j = 0;
	CANFrame *pCANFrameTemp = NULL;
	unsigned int tempCANId = 0;
	unsigned char rx[1000] = {0};
	unsigned char tx[1000] = {0};
	uint8_t buff[34];
	
	/* 初始化要发送的SPI数据 */
	//initSendFrame();
	/* 打开SPI设备 */
	fd = open(device, O_RDWR);
	if (fd < 0)
		printf("can't open device");

	/*
	 * spi mode
	 */
	 uint8_t mode = 0x1;
	ret = ioctl(fd, SPI_IOC_WR_MODE, &mode);
	if (ret == -1)
		printf("can't set spi mode");

	ret = ioctl(fd, SPI_IOC_RD_MODE, &mode);
	if (ret == -1)
		printf("can't get spi mode");

	/*
	 * bits per word
	 */
	ret = ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
	if (ret == -1)
		printf("can't set bits per word");

	ret = ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &bits);
	if (ret == -1)
		printf("can't get bits per word");

	/*
	 * max speed hz
	 */
	ret = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
	if (ret == -1)
		printf("can't set max speed hz");

	ret = ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed);
	if (ret == -1)
		printf("can't get max speed hz");

	printf("spi mode: 0x%x\n", mode);
	printf("bits per word: %u\n", bits);
	printf("max speed: %u Hz (%u kHz)\n", speed, speed/1000);
	
//	usleep(300000);
	printf("start\n");
//	while(1)
//	{
	
	int num = read(fd,buff,34);
	if(num <0)
	{
	printf("no \n");
	}
	else{
	/*
	printf("buf is ");

	printf("ADC_5VA:%.02f ",(buff[0]<<8 | buff[1])* 2.0 /1000);  //5v_A
	printf("ADC_5VB:%.02f ",(buff[2]<<8 | buff[3])* 2.0 /1000);  //5V_B
	printf("ADC_20VA:%.02f ",(buff[4]<<8 | buff[5])* 7.8 /1000);  //20V_A
	printf("ADC_20VB:%.02f ",(buff[6]<<8 | buff[7])* 7.8 /1000);  //20V_B
	printf("ADC_3V3AO:%.02f ",(buff[8]<<8 | buff[9])* 1.1 /1000);  //3V3AO
	printf("ADC_3V3:%.02f ",(buff[10]<<8 | buff[11])* 1.1 /1000);  //3V3
	printf("ADC_3V8:%.02f ",(buff[12]<<8 | buff[13])* 1.1 /1000);  //3V8
	printf("ADC_1V8:%.02f ",(buff[14]<<8 | buff[15])* 1.1 /1000);  //1V8
	printf("ADC_+5V:%.02f ",(buff[16]<<8 | buff[17])* 2.0 /1000);  //+5V
	printf("ADC_12V:%.02f ",(buff[18]<<8 | buff[19])* (13.3/3.3) /1000);  //12V
	printf("ADC_3V3FPGA:%.02f ",(buff[20]<<8 | buff[21])* 1.1 /1000);  //3V3_FPGA
	printf("ADC_1V8FPGA:%.02f ",(buff[22]<<8 | buff[23])* 1.1 /1000);  //1V8_FPGA
	printf("ADC_1V0:%.02f ",(buff[24]<<8 | buff[25])* 1.1 /1000);  //1V0
*/
	printf("版本号：%02d ",((buff[26]<<8 | buff[27]) << 16) | (buff[28]<<8 | buff[29]));
	printf("硬件：%02d ",(buff[30]<<8 | buff[31]));
	printf("软件：%02d ",(buff[32]<<8 | buff[33]));
/*	printf("state：%02d ",(buff[34]<<8 | buff[35]));
	printf("state：%02d ",(buff[36]<<8 | buff[37]));
	printf("state：%02d ",(buff[38]<<8 | buff[39]));
*/


	// for(char i=0; i<sizeof(buff); i+=2)
	// {
	// printf("%02d ",buff[i]<<8 |  buff[i+1]);
	// }
	printf("\n");
	}
//	usleep(100000);
//	}
/*
	spi_frame_tx_.rolling_count = rolling_cnt_++;
		memcpy(tx, (char *)&spi_frame_tx_, 1000);
		ret = transfer(fd, tx, rx, 1000);
		send_src_id_cnt++;
		memcpy((char *)&spi_frame_rx_, rx, 1000);
		for(unsigned int i=0; i<1000; i++)
		{
			printf("%02x ", rx[i]);
		}*/

/*
	while (1)
	{
		//发送SPI数据，并获得接收到的SPI数据
		spi_frame_tx_.rolling_count = rolling_cnt_++;
		memcpy(tx, (char *)&spi_frame_tx_, 1000);
		ret = transfer(fd, tx, rx, 1000);
		send_src_id_cnt++;
		// printf("send 0x81 cnt = %d\r\n", send_src_id_cnt);
		memcpy((char *)&spi_frame_rx_, rx, 1000);
		//for(unsigned int i=0; i<1000; i++)
		//{
		//	printf("%02x ", rx[i]);
		//}
		if(0 == ret)
		{
			//对收到的SPI数据内容进行解析并显示其中的CAN帧内容
			parse_ret = SPIFrameParse(&spi_frame_rx_);
			if(0 == parse_ret)	// SPI帧解析无误
			{
				recvCanFrameCnt = spi_frame_rx_.data_len;		// 获取SPI帧中有效CAN帧个数
				pCANFrameTemp = &spi_frame_rx_.can_frame_buf[0];// 获取SPI帧中有效CAN帧数据内容首地址
				//循环获取有效的CAN帧数据内容并解析 
				for(i=0; i<recvCanFrameCnt; i++)
				{
					tempCANId = (pCANFrameTemp->id[3]<<24)|(pCANFrameTemp->id[2]<<16)|(pCANFrameTemp->id[1]<<8)|(pCANFrameTemp->id[0]);	// 计算CAN ID
					// printf("id = %d\r\n", tempCANId);
					if((MCU_VERSION_REQ_ID != tempCANId) && (MCU_STATUS_REQ_ID != tempCANId) && (MCU_ERROR_REQ_ID != tempCANId))	// 不显示特定CAN ID的内容
					{
						printf("Receive Can Frame: %02x--%02x%02x%02x%02x--%x--%02x %02x %02x %02x %02x %02x %02x %02x\n", pCANFrameTemp->channel, \
						pCANFrameTemp->id[3],pCANFrameTemp->id[2],pCANFrameTemp->id[1],pCANFrameTemp->id[0], \
						pCANFrameTemp->len, \
						pCANFrameTemp->data[0],pCANFrameTemp->data[1],pCANFrameTemp->data[2],pCANFrameTemp->data[3], \
						pCANFrameTemp->data[4],pCANFrameTemp->data[5],pCANFrameTemp->data[6],pCANFrameTemp->data[7]);
					}
					pCANFrameTemp++;	// 获取下一个有效CAN帧数据内容首地址
				}
				printf("rolling cnt = %d\r\n", spi_frame_rx_.rolling_count);
			}
			else if(SPIFRAME_CHECKSUM_CHECK_ERROR == parse_ret)	// SPI帧checksum出错
			{
				SPI_checksum_error_count++;
				printf("checksum error cnt = %d\r\n", SPI_checksum_error_count);
			}
			else	// SPI帧其他错误
			{
				SPI_Other_error_count++;
				printf("other error cnt = %d\r\n", SPI_Other_error_count);
			}
		}
		usleep(5000);
	}
	*/
	close(fd);

	return ret;
}











