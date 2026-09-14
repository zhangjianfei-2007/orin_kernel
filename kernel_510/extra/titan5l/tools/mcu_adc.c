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
#define BYTE_LEN		256


static const char *device = "/dev/spidev1.0";
static uint8_t bits = 8;
static uint32_t speed = 10000000;

//char spi_frame_TX[BYTE_LEN] = {0X99,0X81,0x02,0x03,0x00,0x00,0x06,0x00,0x08,0x00};
//char spi_frame_TX[BYTE_LEN] = {0X99,0X81,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03};
char spi_frame_TX[BYTE_LEN] = {0X78,0X56,0x00,0x00,0x00,0x00,0x00,0x00,0x08,0x09,0x00,0x00};

//char spi_frame_TX[BYTE_LEN] = {0X11,0X22,0x33,0x44,0x00,0x00,0x00,0x00,0x00,0x00};
char spi_frame_RX[BYTE_LEN] = {0X50,0X0A};

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

int main(int argc, char *argv[])
{
	int ret = 0;
	int fd;
	int k = 0;
	
	unsigned char rx[BYTE_LEN] = {0};
	unsigned char tx[BYTE_LEN] = {0};
	
	/* 初始化要发送的SPI数据 */
	/* 打开SPI设备 */
	/*for(k = 0;k<BYTE_LEN;k++)
	{
	spi_frame_TX[k] = k;
	}*/
	fd = open(device, O_RDWR);
	if (fd < 0)
	{
		printf("can't open device");
		return (ret = -1);
	}

	for(k = 0;k<10;k++)
	//while(1)
	{
		//发送SPI数据，并获得接收到的SPI数据
		memcpy(tx, (char *)&spi_frame_TX, BYTE_LEN);
		ret = transfer(fd, tx, rx, BYTE_LEN);

		memcpy((char *)&spi_frame_RX, rx, BYTE_LEN);
#if 0
		for(unsigned int i=0; i<30; i++)
		{
			printf("%02x ", rx[i]);
		}
#endif		
		if((0x78 == rx[0])&&(0x56 == rx[1]))
		{
			printf("------------\n");
			printf("ADC_KL15 = %02dmv ADC_12V=%02dmv\n",(((rx[2]<<8)|rx[3])*36300/256+160),(((rx[4]<<8)|rx[5])*13298/256+160));
			printf("ADC_5V = %02dmv ADC_20V=%02dmv ADC_3V3=%02dmv ADC_1V8=%02dmv\n",(((rx[6]<<8)|rx[7])*6600/256+160),(((rx[8]<<8)|rx[9])*25740/256+160),(((rx[10]<<8)|rx[11])*3630/256+160),(((rx[12]<<8)|rx[13])*3630/256+160));
			printf("ADC1 = %02dmv ADC2=%02dmv ADC3=%02dmv ADC4=%02dmv\n",(((rx[14]<<8)|rx[15])*36300/256+160),(((rx[16]<<8)|rx[17])*36300/256+160),(((rx[18]<<8)|rx[19])*36300/256+160),(((rx[20]<<8)|rx[21])*36300/256+160));
			printf("------------\n");
		}
						//printf("XXX\n ");
		usleep(5000);
	}
	close(fd);

	return ret;
}
