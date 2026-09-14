#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h> 
#include <linux/spi/spidev.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <linux/rtc.h>
#include "log.h"

#define YEAR_ADDR 0x30
#define GPRMC_STATES 0x06
#define SPI_DEVICE "/dev/spidev2.0"

#define RTC_N 0

#define EN_LOG 1   	/* 控制是否记录log  */
#define CYCLIC_UPDATE 1 /* 控制是否循环更新*/

int execShell(char *shellCmd)
{
	int ret = -1;

	if ((NULL == shellCmd) || (strlen(shellCmd) <= 0)) {
		printf("%s parameter shellCmd error \n", __FUNCTION__);
		return -1;
	}

	signal(SIGCHLD, SIG_DFL);

	ret = system(shellCmd);
	if (0 != ret) {
		printf("%s system(%s) error %d\n", __FUNCTION__, shellCmd, errno);
	}

	signal(SIGCHLD, SIG_IGN);

	return ret;
}

int timeDateCtl(void)
{
	int ret = 0;
	char buf[128] = {0};

	sprintf(buf, "%s", "timedatectl set-local-rtc 0");
	ret = execShell(buf);

	return ret;
}

int updateHwClock(void)
{
	int ret = 0;
	char buf[128] = {0};

	sprintf(buf, "%s%d", "hwclock -w -f /dev/rtc", RTC_N);

	ret = execShell(buf);

	return ret;
}

int setWallClockFromHwClock(void)
{
	int ret = 0;
	char buf[128] = {0};

	sprintf(buf, "%s%d", "hwclock -s -f /dev/rtc", RTC_N);

	ret = execShell(buf);

	return ret;
}

int getHwClock(struct rtc_time *rtc_tm)
{
	int ret = 0;
	int fd;

	char buf[128] = {0};
	sprintf(buf, "%s%d", "/dev/rtc", RTC_N);
	fd=open(buf, O_RDONLY);

	if(fd<0)
	{
		return -1;
	}

	ret=ioctl(fd, RTC_RD_TIME, rtc_tm);

    close(fd);

	return ret;
}

bool isHwClockColdReset(void)
{
	struct rtc_time rtc_tm;

	if(getHwClock(&rtc_tm)<0)
		return true;

	printf("hw clock is %d-%d-%d %d:%d:%d\n",
			rtc_tm.tm_year+1900,rtc_tm.tm_mon+1,rtc_tm.tm_mday,
			rtc_tm.tm_hour,rtc_tm.tm_min,rtc_tm.tm_sec);

	/*
	 * for max77620/max20024 pmic on p2888
	 * cold reset: 2000-01-01
	 *
	 * for NVIDIA Voltage Regulator Power Sequencer (PMIC) on p3701
	 * cold reset: 1970-01-01
	 */
	if((rtc_tm.tm_year==100 || rtc_tm.tm_year==70) && rtc_tm.tm_mon==0 && rtc_tm.tm_mday==1){
		return true;
	}

	return false;
}

int main(int argc,char *argv[])
{
        int m_delay=500;
        int ret = 0;
        int fd,msec,usec;
	int gprmc_lock = 0;
	long int time1,time2,time3,time4,time_gps,time_update;
        struct timeval tv,tv_sys;
	struct timezone tz;
        struct tm tm;

	int limit = 0; /* 用于限制更改系统时间次数 */

        uint32_t mode = 0;
        uint8_t bits = 8;
        uint32_t speed = 25000000;

        bool lock_once=false;
        bool upHwClkOnce=false;

        if(getuid()!=0){  /* 判断是否是root用户 */
                printf("The user isn`t root ,please use this program with sudo\n");
                return -1;
        }
	printf("date_update v3.0.0\n");


        static uint8_t default_tx[17] = {
        	'S', 'P', 'I', 'R', 0x10, YEAR_ADDR,
                0,1,2,3,4,5,6,7,8,9,
	};

	static uint8_t default_rx[16] = {
                0,0,0,0,0,0,18,6,2,12,45,58,0x2,0x2b,0x1,0xbc,
        };
       
#if EN_LOG
	if(access("/var/log/date.log",F_OK)){
                system("touch /var/log/date.log");		/* 日志文件不存在的话新建文件 */
        }else{
                system("mv /var/log/date.log /var/log/date_bak.log ");  /* 日志文件存在的话 备份到新文件 */
                system("touch /var/log/date.log");
        }

        FILE* pFile = fopen("/var/log/date.log", "a");
        write_log(pFile, "%s\n", "date_update program start to  running");
        printf("%s\n", "date_update program start to  running");

#endif
        ret=timeDateCtl();

    	if(isHwClockColdReset()){
    		write_log(pFile, "hw clock cold reset performed.");
    		//since hw clock is invalid, cannot set wall clock from hw clock.
    		//todo: inform UI, let user set wall clock manually.
    	}else{
    		write_log(pFile, "hw clock(s) updated since last cold reset.");
    		ret=setWallClockFromHwClock();
    	}

        fd = open(SPI_DEVICE, O_RDWR);		/* 打开SPIDEV设备 */
        ret = ioctl(fd, SPI_IOC_WR_MODE32, &mode);
        ret = ioctl(fd, SPI_IOC_RD_MODE32, &mode);
        ret = ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
        ret = ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &bits);
        ret = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
        ret = ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed);
	if(fd<0||ret<0){
		printf("spi device open error!!!!\n");
		return -1;
      }
        struct spi_ioc_transfer tr = {  	/* 准备 tr结构体 设置输入输出buf，传输长度、速度等信息 */
                .tx_buf = (unsigned long)default_tx,
                .rx_buf = (unsigned long)default_rx,
                .len = 16,
                .delay_usecs = 0,
                .speed_hz = speed,
                .bits_per_word = bits,
        };
do {
	usleep(m_delay*1000);
	default_tx[5] = GPRMC_STATES;
	default_rx[6] = 0;
    ret=ioctl(fd, SPI_IOC_MESSAGE(1), &tr);       /* 从FPGA读取锁定信息 */
	if (ret < 0) {
		write_log(pFile, "%s\n", "spi transfer GPRMC failed");
		continue;
	}
	//gprmc_lock = default_rx[6]&0x08;
	gprmc_lock = default_rx[6]&0x0c;

	//if(!gprmc_lock)				/* 如果未锁定，跳出此次循环，再次判断 */
	if(gprmc_lock != 0x0c)
		continue;
#if EN_LOG
        write_log(pFile, "%s\n", "GPRMC locked");
	printf("%s\n", "GPRMC locked");
#endif
	default_tx[5] = YEAR_ADDR;
    ret=ioctl(fd, SPI_IOC_MESSAGE(1), &tr);      /* 从FPGA 读取时间信息 */
	if (ret < 0) {
		write_log(pFile, "%s\n", "spi transfer DATE failed");
		continue;
	}
	tm.tm_year = default_rx[6]+2000-1900;
	tm.tm_mon = default_rx[7]-1;
        tm.tm_mday = default_rx[8];
        tm.tm_hour = default_rx[9];
        tm.tm_min = default_rx[10];
        tm.tm_sec = default_rx[11];

        msec = default_rx[12]*256+default_rx[13];
        usec = default_rx[14]*256+default_rx[15];

        tv.tv_sec = timegm(&tm);                  /* 年月日时分秒转换成时间戳的秒部分 */
        tv.tv_usec = msec*1000+usec;		 /* 毫秒和微秒转化成时间戳的微妙部分 */

        ret=settimeofday(&tv,NULL); 		/* 修改系统时间 */
    	if (ret < 0) {
    		write_log(pFile, "%s\n", "settimeofday failed.");
    		continue;
    	}
#if CYCLIC_UPDATE	
        ret=gettimeofday(&tv_sys,NULL);		/* 获取系统时间 */
    	if (ret < 0) {
    		write_log(pFile, "%s\n", "gettimeofday failed in first circle.");
    		continue;
    	}
        time1 = tv_sys.tv_sec*1000000+tv_sys.tv_usec;

        ret=ioctl(fd, SPI_IOC_MESSAGE(1), &tr);     /* 获取GPS时间 */
    	if (ret < 0) {
    		write_log(pFile, "%s\n", "spi transfer DATE failed in first circle.");
    		continue;
    	}
        tm.tm_year = default_rx[6]+2000-1900;
        tm.tm_mon = default_rx[7]-1;
        tm.tm_mday = default_rx[8];
        tm.tm_hour = default_rx[9];
        tm.tm_min = default_rx[10];
        tm.tm_sec = default_rx[11];

        msec = default_rx[12]*256+default_rx[13];
        usec = default_rx[14]*256+default_rx[15];

        tv.tv_sec = timegm(&tm);
        tv.tv_usec = msec*1000+usec;

        time_gps = tv.tv_sec*1000000+tv.tv_usec;

	if ( abs(time_gps-time1) > 10000 ){         /* 系统时间和GPS时间差值小于10ms 表示修改成功 */
#if EN_LOG
        write_log(pFile, "time_gps-time_sys=%ld\t time_gps=%ld\t,time_sys=%ld\t\n",time_gps-time1,time_gps,time1);
	printf("time_gps-time_sys=%ld\t time_gps=%ld\t,time_sys=%ld\t\n",time_gps-time1,time_gps,time1);
#endif
		limit++;
		/*如果系统时间5次写不成功 直接结束程序*/
		if (limit>=5){
        		//return -1;
		}
		continue;
	}
#endif


#if EN_LOG
	write_log(pFile, "%s\n", "date has updated");
	printf("%s\n", "date has updated");
#endif
	lock_once=true;
	break;


}while(1);  



#if CYCLIC_UPDATE
do{

	usleep(m_delay*4*1000);

 	ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
        tm.tm_year = default_rx[6]+2000-1900;
        tm.tm_mon = default_rx[7]-1;
        tm.tm_mday = default_rx[8];
        tm.tm_hour = default_rx[9];
        tm.tm_min = default_rx[10];
        tm.tm_sec = default_rx[11];
        msec = default_rx[12]*256+default_rx[13];
        usec = default_rx[14]*256+default_rx[15];
        tv.tv_sec = timegm(&tm);
        tv.tv_usec = msec*1000+usec;
        time1 = tv.tv_sec*1000000+tv.tv_usec;
	//printf("%s%ld\n", "FPGA时间", time1);

	gettimeofday(&tv_sys,NULL);
	time2 = tv_sys.tv_usec;
	//printf("%s%ld\n", "系统时间", time2);

        ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
        tm.tm_year = default_rx[6]+2000-1900;
        tm.tm_mon = default_rx[7]-1;
        tm.tm_mday = default_rx[8];
        tm.tm_hour = default_rx[9];
        tm.tm_min = default_rx[10];
        tm.tm_sec = default_rx[11];
        msec = default_rx[12]*256+default_rx[13];
        usec = default_rx[14]*256+default_rx[15];
        tv.tv_sec = timegm(&tm);
        tv.tv_usec = msec*1000+usec;
	time_gps = tv.tv_sec*1000000+tv.tv_usec;
	//printf("%s%ld\n", "FPGA时间", time_gps);

		/*连续读两次FPGA，如果超过1ms,判断FPGA时间出故障*/
        if( abs(time_gps-time1) > 10000 ){
                continue;
        }
		/*判断FPGA时间是否为默认值*/
        if( (120 == tm.tm_year) && (0 ==  tm.tm_mon) && (1 == tm.tm_mday)){
		printf("%s\n", "fffffffffffffffffffffffffffffffffffffffffff");
                continue;
        }

	gettimeofday(&tv_sys,NULL);
	time1 = tv_sys.tv_usec;
	//printf("%s%ld\n", "系统时间", time1);	
	time3=time1-time2;
	//printf("%s%ld\n", "程序运行时间", time3);	
	if(time3 < 0)
	{
		time3=time3+1000000;
	//printf("%s%ld\n", "补偿后程序运行时间", time3);			
	}
	time_update = time_gps+time3;
	//printf("%s%ld\n", "准备更新时间", time_update);	
	tv_sys.tv_sec = time_update/1000000;
	tv_sys.tv_usec = time_update%1000000;
	settimeofday(&tv_sys,NULL);

	printf("date_update\n");

	//if FPGA date ever locked once(system wall clock also updated), and hardware clock never updated,
	//update hw clock from system wall clock.
	if(lock_once && !upHwClkOnce){
		ret=updateHwClock();
		if(ret >= 0){
			write_log(pFile, "hardware clock updated.\n");
			upHwClkOnce=true;
		}
	}
}while(1);
#endif

        close(fd);
#if EN_LOG
        fclose(pFile);
#endif
        return ret;
}
