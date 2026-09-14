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
#include <string.h>
#include <time.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include "titan4_io.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

#define I2C_DEV0 "/dev/i2c-0"   
#define I2C_DEV8 "/dev/i2c-8"   

static uint32_t mode=0;
static uint8_t bits = 8;
static uint32_t speed = 25000000;

struct GPS_date gps_date;
static int spi_init()
{      
	int ret = 0;
	int fd;
	static const char *device = "/dev/spidev1.0";

        fd = open(device, O_RDWR);
        ret = ioctl(fd, SPI_IOC_WR_MODE32, &mode);
        ret = ioctl(fd, SPI_IOC_RD_MODE32, &mode);
        ret = ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
        ret = ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &bits);
        ret = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
        ret = ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed);
	if(fd<0||ret<0){
		printf("device open error!!!!\n");
		return -1;
	}else
		return fd;


}

static void transfer(int fd, uint8_t const *tx, uint8_t const *rx, size_t len)
{
        int ret;
        int out_fd;
        struct spi_ioc_transfer tr = {
                .tx_buf = (unsigned long)tx,
                .rx_buf = (unsigned long)rx,
                .len = len,
                .delay_usecs = 0,
                .speed_hz = speed,
                .bits_per_word = bits,
        };

        if (mode & SPI_TX_QUAD)
                tr.tx_nbits = 4;
        else if (mode & SPI_TX_DUAL)
                tr.tx_nbits = 2;
        if (mode & SPI_RX_QUAD)
                tr.rx_nbits = 4;
        else if (mode & SPI_RX_DUAL)
                tr.rx_nbits = 2;
        if (!(mode & SPI_LOOP)) {
                if (mode & (SPI_TX_QUAD | SPI_TX_DUAL))
                        tr.rx_buf = 0;
                else if (mode & (SPI_RX_QUAD | SPI_RX_DUAL))
                        tr.tx_buf = 0;
        }

        ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
        if (ret < 1)
                printf("can't send spi message");

}

int gpi_get(int id)
{
	static uint8_t default_tx[] = {
        	'S', 'P', 'I', 'R', 0x01, 0x05,'x',
	};

	static uint8_t default_rx[ARRAY_SIZE(default_tx)] = {0, };
	int fd,ret;
	fd=spi_init();
	if(id<0)
		return -1;
        transfer(fd, default_tx, default_rx, sizeof(default_tx));
       switch(id)
       {
	       
	       case 1 :
		       ret=(default_rx[6]&0x04)>>2;
		       break;
	       case 2 :
		       ret=(default_rx[6]&0x08)>>3;
		       break;
	       case 3 :
		       ret=(default_rx[6]&0x10)>>4;
		       break;
	       case 4 :
		       ret=(default_rx[6]&0x20)>>5;
		       break;
		default :
		       printf("ID errori\n");
		       break;
       } 
        close(fd);

        return ret;
}

int led_ctrl(int id ,int value)
{
        static uint8_t ctrl_data;

	 uint8_t default_tx[] = {
       		'S', 'P', 'I', 'R', 0x01, 0x1b,
        	0x78,
	};

		uint8_t default_rx[] = {0, };
        static int fd,ret;
        
        if(id!=1&&id!=2&&id!=3){
                printf("please input correct ID  1 or 2  or 3\n ");
                return -1 ;
        }
        if(value!=0&&value!=1&&value!=2&&value!=3){
                printf("please input correct value  0 or 1 or 3 or 4  \n");
                return -1;
        }
		

        fd = spi_init();
        if (fd < 0){
		return -1;
	}

        transfer(fd, default_tx, default_rx, 7);
        ctrl_data=default_rx[6];
	switch(id){
	
	case 1:
		ctrl_data=ctrl_data&0xFC|(value<<0);
		break;
	
	case 2:
		ctrl_data=ctrl_data&0xF3|(value<<2);
		break;
	case 3:
		ctrl_data=ctrl_data&0xCF|(value<<4);
		break;
	
	}


        default_tx[3]='W';
        default_tx[6]=ctrl_data;

        transfer(fd, default_tx, default_rx, 9);
        
	close(fd);
        return ret;
}


int gpo_ctrl(int id ,int value)
{
        	static uint8_t ctrl_data;

	static uint8_t default_tx[] = {
       		'S', 'P', 'I', 'R', 0x01, 0x0a,
        	0x00,
	};

	static	uint8_t default_rx[] = {0, };
        static int fd,ret;
        
        if(id!=1&&id!=2){
                printf("please input correct ID  1 or 2 \n ");
                return -1 ;
        }
        if(value!=1&&value!=0){
                printf("please input correct value  0 or 1  \n");
                return -1;
        }
		

        fd = spi_init();
        if (fd < 0){
		return -1;
	}
        transfer(fd, default_tx, default_rx, 7);
        transfer(fd, default_tx, default_rx, 7);
        ctrl_data=default_rx[6];

        if(1==id){
                if(1==value)
                        ctrl_data|=0x04;
                else
                        ctrl_data&=0xFB;
        }else if (2==id){
                if(1==value)
                        ctrl_data|=0x08;
                else
                        ctrl_data&=0xF7;

        }


        default_tx[3]='W';
        default_tx[6]=ctrl_data;

        transfer(fd, default_tx, default_rx, 7);
        transfer(fd, default_tx, default_rx, 7);

        default_tx[3]='R';
        transfer(fd, default_tx, default_rx, 7);
        transfer(fd, default_tx, default_rx, 7);
        
	close(fd);
        return ret;
}
char * get_key()
{
	static char key[9]={'A', 'B', 'C','D','T','i','t','a','n'};
	static uint8_t ctrl_data;

	static uint8_t default_tx[] = {
       		'S', 'P', 'I', 'W', 0x03, 
		0x17,0x00,0x00,0x00,
	};

	static	uint8_t default_rx[] = {0, };
        static int fd,ret;
       	uint8_t word1,word2,word3,key1,key2,temp;
	srand((unsigned)time(NULL));
	word1=(uint8_t)rand();
	word2=(uint8_t)rand();
	word3=(uint8_t)rand();
	default_tx[6]=word1;
	default_tx[7]=word2;
	default_tx[8]=word3;
	

	temp=word1;
	word1=word1<<3|word2>>5;
	word2=word2<<3|word3>>5;
	word3=word3<<3|temp>>5;
	key1=word1^word2^word3;	


        fd = spi_init();
        if (fd < 0){
			goto err;
		}
        transfer(fd, default_tx, default_rx, 9);
        transfer(fd, default_tx, default_rx, 9);

        default_tx[3]='R';
        default_tx[5]=0x1A;

        transfer(fd, default_tx, default_rx, 9);
        transfer(fd, default_tx, default_rx, 9);
	close(fd);
	key2=default_rx[6];

	if(key1==key2){
		key[0]='H';
		key[1]='y';
		key[2]='z';
		key[3]='x';
	}
	else{
	
	err:
		key[0]='A';
		key[1]='B';
		key[2]='C';
		key[3]='D';
	}
        
        return   key;
}


int dac_ctrl(int id , float value)
{
	static struct dac_dev{
		int bus;
		char device;
		float  data;
	}dev;




	switch(id){
		case 1:
			dev.bus=0;
			dev.device=0x0c;
			dev.data=value;
			break;
		case 2:
			dev.bus=0;
			dev.device=0x0e;
			dev.data=value;
			break;
		case 3:
			dev.bus=8;
			dev.device=0x0c;
			dev.data=value;
			break;
		case 4:
			dev.bus=8;
			dev.device=0x0e;
			dev.data=value;
			break;

		default :

			printf("please input correct ID \n");
			return -1;
			break;

	
	}

	static int fd;
	static int res;
	static char reg_buf[2],read_buf[4],write_buf[4];
	static float voltage=0.0;
	if(0==dev.bus){
		fd = open(I2C_DEV0, O_RDWR);
		if(fd < 0){
			printf("####i2c device open failed####\n");
			return (-1);
		}
	
	}else{
		fd = open(I2C_DEV8, O_RDWR);
		if(fd < 0){
			printf("####i2c device open failed####\n");
			return (-1);
		}
	
	}

	res = ioctl(fd,I2C_TENBIT,0);
	res =ioctl(fd,I2C_SLAVE,dev.device); 

	write_buf[1]=((uint16_t)(dev.data/10.0*0x0FFF))&0x00FF;
	write_buf[0]=(uint16_t)(dev.data/10.0*0x0FFF)>>8;

		res=write(fd,write_buf,2);
		if(res<0){
			printf("write data error\n");
			return -1;
		}

	close(fd);
	return 0;

}


float adc_get( int id)
{
	static struct adc_dev{
        	float adc[4];
	}dev;

	static int fd;
        static int res;
        static char reg_buf[2],read_buf[4],write_buf[4];
	uint16_t temp;
        static float voltage=0.0;

        fd = open(I2C_DEV8, O_RDWR);
        if(fd < 0){
                printf("####i2c device open failed####\n");
                return (-1);
        }

        res = ioctl(fd,I2C_TENBIT,0);
        res =ioctl(fd,I2C_SLAVE,0x49); 



        write_buf[0]=0x01;
        write_buf[2]=0x83;
        for(int i=0;i<4;i++ ){
                write_buf[1]=0xC3+(i<<4);
        
                res=write(fd,write_buf,3);
                if(res<0){
                        printf("write data error\n");
                        return -1;
                }


                reg_buf[1]=0x00;
                res=write(fd,reg_buf,1);
                if(res<0){
                        printf("write reg_buf2 error\n");
                        return -1;
                }

                res=read(fd,read_buf,1);
                if(res<0){
                        printf("read  data error\n");
                        return -1;
                }
                dev.adc[i] = (( float)read_buf[0])/0x7F*10;

        }

	switch(id)
	{	
		case 1:
			voltage = dev.adc[1];
			break;
			
		case 2:
			voltage = dev.adc[2];
			break;
			
		case 3:
			voltage = dev.adc[3];
			break;
	
		case 4:
			voltage = dev.adc[0];
			break;
	
		default :
                        printf("please input correct ID \n");
                        voltage =-1;
                        break;

	
	}

	close(fd);
        return voltage;

}


struct GPS_date get_date( )
{

    int fd;
    uint8_t default_tx[16] = {
        	'S', 'P', 'I', 'R', 0x10, 0x30,
                0,1,2,3,4,5,6,7,8,9
	};

	uint8_t default_rx[16] = {
                0,0,0,0,0,0,18,6,2,12,45,58,0x2,0x2b,0x1,0xbc
    };

        fd = spi_init();
		if(fd<0){
        	gps_date.year = 0;
        	gps_date.month= 0;
        	gps_date.day = 0;
        	gps_date.hour = 0;
        	gps_date.minute = 0;
        	gps_date.second = 0;
			gps_date.millisecond = 0;
        	gps_date.microsecond = 0;
		}else{
        	transfer(fd, default_tx, default_rx, 16);
			gps_date.year = default_rx[6]+2000;
			gps_date.month= default_rx[7];
			gps_date.day = default_rx[8];
			gps_date.hour = default_rx[9];
			gps_date.minute = default_rx[10];
			gps_date.second = default_rx[11];
			gps_date.millisecond = default_rx[12]*256+default_rx[13];
			gps_date.microsecond = default_rx[14]*256+default_rx[15];
		}

        return gps_date;
}