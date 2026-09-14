 
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
 
#include <getopt.h>             /* getopt_long() */
#include <errno.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <poll.h>
#include <time.h>
#include <stdarg.h>

//设置端口号、波特率、
char* dev_name = "/dev/ttyTHS0";
int baudrate = 115200;
int flow_ctrl_flag = 0;
int show_hex_flag = 1;
char gpio_path[100]="/sys/class/gpio/PP.00";    // PPS 1.8V Signal gpio direction
struct PPS_Lock
{
    int PPSTimeArr[3];      /* continue pps */
    _Bool Lock;     
    struct timeval gprmc;   /* gprmc Sec timestamp */
};

static int gpio_config(const char *attr,const char *val){
    char file_path[100];
    int len;
    int fd;

    sprintf(file_path,"%s/%s",gpio_path,attr);
    if(0 > (fd = open(file_path,O_WRONLY))){
        perror("config open error");
        return fd;
    }
    len = strlen(val);
    if(len !=write(fd,val,len)){
        perror("config write error");
        return -1;
    }
    close(fd);
    return 0;
}
static void errno_exit(const char *s)
{
        fprintf(stderr, "%s error %d, %s\\n", s, errno, strerror(errno));
        exit(EXIT_FAILURE);
}
 
/**
 * \brief 打开串口设备
 *
 * \param[in] p_path：设备路径
 *
 * \retval 成功返回文件描述符，失败返回-1
 */
int uart_open(const char *p_path)
{
    /* O_NOCTTY：阻止操作系统将打开的文件指定为进程的控制终端,如果没有指定这个标
                 志，那么任何一个输入都将会影响用户的进程。 */
    /* O_NONBLOCK：使I/O变成非阻塞模式，调用read如果没有接收到数据则立马返回-1，
            并且设置errno为EAGAIN。*/
    /* O_NDELAY： 与O_NONBLOCK完全相同 */
    return open(p_path, O_RDWR | O_NOCTTY);
}
 
/**
 * \brief 测试函数，打印struct termios各成员值
 */
static void __print_termios(struct termios *p_termios)
{
    printf("c_iflag = %#010x\n", p_termios->c_iflag);
    printf("c_oflag = %#010x\n", p_termios->c_oflag);
    printf("c_cflag = %#010x\n", p_termios->c_cflag);
    printf("c_lflag = %#010x\n\n", p_termios->c_lflag);
}
 
/**
 * \brief 设置串口属性
 *
 * \param[in] fd：打开的串口设备的文件描述符
 * \param[in] baudrate：波特率
 *            #{0, 50, 75, 110, 134, 150, 200, 300, 600, 1200, 1800,
 *              2400, 4800, 9600, 19200, 38400, 57600, 115200, 230400}
 * \param[in] bits：数据位
 *            #{5, 6, 7, 8}
 * \param[in] parity：校验
 *            #'n'/'N'：无校验
 *            #'o'/'O'：奇校验
 *            #'e','E'：偶校验
 * \param[in] stop：停止位
 *            #1：1个停止位
 *            #2：2个停止位
 * \param[in] flow：流控
 *            #'n'/'N'：不使用流控
 *            #'h'/'H'：使用硬件流控
 *            #'s'/'S'：使用软件流控
 *
 * \retval 成功返回0，失败返回-1，并打印失败原因
 *
 * \note 虽然波特率设置支持这么多值，但并不代表输入表中支持的值波特率就
 *       一定能设置成功。
 */
int uart_set(int fd, int baudrate, int bits, char parity, int stop, char flow)
{
    struct termios termios_uart;
    int ret = 0;
    speed_t uart_speed = 0;
 
    /* 获取串口属性 */
    memset(&termios_uart, 0, sizeof(termios_uart));
    ret = tcgetattr(fd, &termios_uart);
    if (ret == -1) {
        printf("tcgetattr failed\n");
        return -1;
    }
 
    //__print_termios(&termios_uart);
 
    /* 设置波特率 */
    switch (baudrate) {
        case 0:      uart_speed = B0;      break;
        case 50:     uart_speed = B50;     break;
        case 75:     uart_speed = B75;     break;
        case 110:    uart_speed = B110;    break;
        case 134:    uart_speed = B134;    break;
        case 150:    uart_speed = B150;    break;
        case 200:    uart_speed = B200;    break;
        case 300:    uart_speed = B300;    break;
        case 600:    uart_speed = B600;    break;
        case 1200:   uart_speed = B1200;   break;
        case 1800:   uart_speed = B1800;   break;
        case 2400:   uart_speed = B2400;   break;
        case 4800:   uart_speed = B4800;   break;
        case 9600:   uart_speed = B9600;   break;
        case 19200:  uart_speed = B19200;  break;
        case 38400:  uart_speed = B38400;  break;
        case 57600:  uart_speed = B57600;  break;
        case 115200: uart_speed = B115200; break;
        case 230400: uart_speed = B230400; break;
        case 460800: uart_speed = B460800; break;
        case 500000: uart_speed = B500000; break;
        case 921600: uart_speed = B921600; break;
        default: printf("Baud rate not supported\n"); return -1;
    }
    cfsetspeed(&termios_uart, uart_speed);
 
    /* 设置数据位 */
    switch (bits) {
        case 5:     /* 数据位5 */
            termios_uart.c_cflag &= ~CSIZE;
            termios_uart.c_cflag |= CS5;
        break;
 
        case 6:     /* 数据位6 */
            termios_uart.c_cflag &= ~CSIZE;
            termios_uart.c_cflag |= CS6;
        break;
 
        case 7:     /* 数据位7 */
            termios_uart.c_cflag &= ~CSIZE;
            termios_uart.c_cflag |= CS7;
        break;
 
        case 8:     /* 数据位8 */
            termios_uart.c_cflag &= ~CSIZE;
            termios_uart.c_cflag |= CS8;
        break;
 
        default:
            printf("Data bits not supported\n");
            return -1;
    }
 
    /* 设置校验位 */
    switch (parity) {
        case 'n':   /* 无校验 */
        case 'N':
            termios_uart.c_cflag &= ~PARENB;
            termios_uart.c_iflag &= ~INPCK;        /* 禁能输入奇偶校验 */
        break;
 
        case 'o':   /* 奇校验 */
        case 'O':
            termios_uart.c_cflag |= PARENB;
            termios_uart.c_cflag |= PARODD;
            termios_uart.c_iflag |= INPCK;        /* 使能输入奇偶校验 */
            termios_uart.c_iflag |= ISTRIP;        /* 除去第八个位（奇偶校验位） */
        break;
 
        case 'e':   /* 偶校验 */
        case 'E':
            termios_uart.c_cflag |= PARENB;
            termios_uart.c_cflag &= ~PARODD;
            termios_uart.c_iflag |= INPCK;        /* 使能输入奇偶校验 */
            termios_uart.c_iflag |= ISTRIP;        /* 除去第八个位（奇偶校验位） */
        break;
 
        default:
            printf("Parity not supported\n");
            return -1;
    }
 
    /* 设置停止位 */
    switch (stop) {
        case 1: termios_uart.c_cflag &= ~CSTOPB; break; /* 1个停止位 */
        case 2: termios_uart.c_cflag |= CSTOPB;  break; /* 2个停止位 */
        default: printf("Stop bits not supported\n");
    }
 
    /* 设置流控 */
    switch (flow) {
        case 'n':
        case 'N':   /* 无流控 */
            termios_uart.c_cflag &= ~CRTSCTS;
            termios_uart.c_iflag &= ~(IXON | IXOFF | IXANY);
        break;
 
        case 'h':
        case 'H':   /* 硬件流控 */
            termios_uart.c_cflag |= CRTSCTS;
            termios_uart.c_iflag &= ~(IXON | IXOFF | IXANY);
        break;
 
        case 's':
        case 'S':   /* 软件流控 */
            termios_uart.c_cflag &= ~CRTSCTS;
            termios_uart.c_iflag |= (IXON | IXOFF | IXANY);
        break;
 
        default:
            printf("Flow control parameter error\n");
            return -1;
    }
 
    /* 其他设置 */
    termios_uart.c_cflag |= CLOCAL;    /* 忽略modem（调制解调器）控制线 */
    termios_uart.c_cflag |= CREAD;    /* 使能接收 */
 
    /* 禁能执行定义（implementation-defined）输出处理，意思就是输出的某些特殊数
       据会作特殊处理，如果禁能的话那么就按原始数据输出 */
    termios_uart.c_oflag &= ~OPOST;
 
    /**
     *  设置本地模式位原始模式
     *  ICANON：规范输入模式，如果设置了那么退格等特殊字符会产生实际动作
     *  ECHO：则将输入字符回送到终端设备
     *  ECHOE：如果ICANON也设置了，那么收到ERASE字符后会从显示字符中擦除一个字符
     *         通俗点理解就是收到退格键后显示内容会往回删一个字符
     *  ISIG：使终端产生的信号起作用。（比如按ctrl+c可以使程序退出）
     */
    termios_uart.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
 
    /**
     * 设置等待时间和最小接收字符
     * 这两个值只有在阻塞模式下有意义，也就是说open的时候不能传入O_NONBLOCK，
     * 如果经过了c_cc[VTIME]这么长时间，缓冲区内有数据，但是还没达到c_cc[VMIN]个
     * 数据，read也会返回。而如果当缓冲区内有了c_cc[VMIN]个数据时，无论等待时间
     * 是否到了c_cc[VTIME]，read都会返回，但返回值可能比c_cc[VMIN]还大。如果将
     * c_cc[VMIN]的值设置为0，那么当经过c_cc[VTIME]时间后read也会返回，返回值
     * 为0。如果将c_cc[VTIME]和c_cc[VMIN]都设置为0，那么程序运行的效果与设置
     * O_NONBLOCK类似，不同的是如果设置了O_NONBLOCK，那么在没有数据时read返回-1，
     * 而如果没有设置O_NONBLOCK，那么在没有数据时read返回的是0。
     */
    termios_uart.c_cc[VTIME] = 1;   /* 设置等待时间，单位1/10秒 */
    termios_uart.c_cc[VMIN]  = 1;    /* 最少读取一个字符 */
 
    tcflush(fd, TCIFLUSH);          /* 清空读缓冲区 */
 
    /* 写入配置 */
    ret = tcsetattr(fd, TCSANOW, &termios_uart);
    if (ret == -1) {
        printf("tcsetattr failed\n");
    }
 
    return ret;
}

 
static void usage(FILE *fp, int argc, const char **argv)
{
    fprintf(fp,
             "Usage: %s [options]\n"
             "default device name: /dev/ttyTHS0\n"
             "default baud rate: 115200\n"
             "default show: hex format\n"
             "default flow ctrl: no\n"
             "Options:\n"
             "-d | --device        uart device name [%s]\n"
             "-h | --help          Print this messagen\n"
             "-b | --baudrate      set baudrate %d\n"
             "-s | --string        format string show\n"
             "-f | --flow_ctrl     add hardware flow ctrl\n"
             "",
             argv[0], dev_name, baudrate);
}
 
static const struct option
long_options[] = {
        { "help",      no_argument,           NULL, 'h' },
        { "device",    required_argument,     NULL, 'd' },
        { "baudrate",  required_argument,     NULL, 'b' },
        { "string",    no_argument,           NULL, 's' },
        { "flow_ctrl", no_argument,           NULL, 'f' },
        { 0, 0, 0, 0 }
};
 
static const char short_options[] = "hd:b:sf";
// 参数解析
void option_par(int argc, const char *argv[])
{
    for (;;) {
            int idx;
            int c;
 
            c = getopt_long(argc, argv,
                            short_options, long_options, &idx);
 
            if (-1 == c)
                    break;
 
            switch (c) {
            case 0: /* getopt_long() flag */
                    break;
 
            case 'd':
                    dev_name = optarg;
                    break;
 
            case 'h':
                    usage(stdout, argc, argv);
                    exit(EXIT_SUCCESS);
 
            case 's':
                    show_hex_flag = 0;
                    printf("format string\r\n");
                    break;
 
            case 'f':
                    flow_ctrl_flag = 1;
                    printf("add hardware flow ctrl\r\n");
                    break;
 
            case 'b':
                    errno = 0;
                    baudrate = strtol(optarg, NULL, 0);
                    printf("set baud rate is : %d\r\n", baudrate);
                    if (errno)
                            errno_exit(optarg);
                    break;
 
            default:
                    usage(stderr, argc, argv);
                    exit(EXIT_FAILURE);
            }
    }
}
 
int main(int argc, const char *argv[])
{
    int fd = 0;
    int ret = 0;
    int pid = 0;
    char buf[4096] = { 0 };
    int len = 0;
    int limit = 0;
    int val = 0;
    int i =0;

	char utc_str[30];
	struct tm tm;
    struct timeval tv,tv_sys;
    long int time1,time2,time_gps,time_update;
    struct pollfd pfd;
    struct timeval tm_sys;
    struct tm *local_tm ;
    char file_path[100]={0};
    struct PPS_Lock pps = {
        .Lock = 0,
        .PPSTimeArr = {0},
    
    };

	if(getuid()!=0)
	{  /* 判断是否是root用户 */
        printf("The user isn`t root ,please use this program with sudo\n");
        exit(-1);
    }
/*ORIN GPIO37 Receive PPS. PPS_IN_12V -> MCU_PPS_IN -> PPS_TO_ORIN -> ORIN_GPIO37_1V8(J54,PP.00,440,1.8V)*/

	if(access(gpio_path,F_OK))       //export PP.00(J54)
	{
        int len;
        int fd;
        if(0 > (fd = open("/sys/class/gpio/export",O_WRONLY))){
            perror("open error");
            exit(-1);
        }
        len = strlen("440");
        if(len !=write(fd,"440",len)){ 
            perror("write error");
            exit(-1);
        }
        close(fd);
    }    
    if(gpio_config("direction","in")){
        exit(-1);
    }
    if(gpio_config("active_low","0")){
        exit(-1);
    }
    sleep(0.1);
    if(gpio_config("edge","rising")){
        exit(-1);
    }
    sprintf(file_path,"%s/%s",gpio_path,"value");

	if(0 > 	(pfd.fd=open(file_path,O_RDONLY)))
	{
		perror("open '/sys/class/gpio/PP.00/value' failed!\n");
		return -1;
	}
    pfd.events=POLLPRI; //care about  highly primary interrupt
    read(pfd.fd,&val,1);    // pfd set zero
    option_par(argc, argv);
 
    /* 打开串口设备 */
    fd = uart_open(dev_name);
    if (fd < 0) {
        printf("open %s failed\n", dev_name);
        return 0;
    }
 
    /**
     * 配置串口：
     * 波特率：baudrate
     * 数据位：8
     * 校验  ：无校验
     * 停止位：1
     * 流控  ：
     */
    ret = uart_set(fd, baudrate, 8, 'n', 1, flow_ctrl_flag ? 'h' : 'n');
    if (ret == -1) {
        return 0;
    }	
	printf("%s baudrate=%d, flow_ctrl_flag=%d\n", dev_name, baudrate, flow_ctrl_flag);
    printf("date update v4.1.0 succeeded.\n");
	while(1)
	{
        /* Set value timeout = 1000ms and supervise PPS rising edge */        
        ret = poll(&pfd,1,-1);
        if(0 > ret){
            perror("poll error");
            exit(-1);
        }

/* PPS interrupt service function */
        if(pfd.revents & POLLPRI){
        if(pps.Lock = 1){
            settimeofday(&tv_sys,NULL);
            }               
            gettimeofday(&tm_sys,NULL);     //get system time
	        time1 = tm_sys.tv_sec*1000000+tm_sys.tv_usec; //Now system time           
            if(0 > lseek(pfd.fd,0,SEEK_SET)){
                perror("lseek error");
                exit(-1);
            }
            if(0 > read(pfd.fd,&val,1)){
                perror("read error");
                exit(-1);
            }
            i%=3;
            pps.PPSTimeArr[i++]=1;    
//            printf("\n pps.PPSTimeArr[%d]=%d\n",i,pps.PPSTimeArr[i]);
            if(pps.PPSTimeArr[0] && pps.PPSTimeArr[1] && pps.PPSTimeArr[2]){
                pps.Lock = 1;
                memset(buf, 0, sizeof(buf));
                len = read(fd, buf, sizeof(buf));
                if (len > 0) 
                {
                    tm.tm_year = (buf[57] - 48) * 10 + buf[58]-48 + 2000 - 1900;
                    tm.tm_mon = (buf[55] - 48) * 10 + buf[56]-48 - 1;
                    tm.tm_mday = (buf[53] - 48) * 10 + buf[54]-48;
                    tm.tm_hour = (buf[7] - 48) * 10 + buf[8]-48;
                    tm.tm_min = (buf[9] - 48) * 10 + buf[10]-48;
                    tm.tm_sec = (buf[11] - 48) * 10 + buf[12]-48;
                    //获取串口信息，转为时间
                    tv.tv_sec = timegm(&tm);
                    tv_sys.tv_sec = tv.tv_sec + 1;
                    tv_sys.tv_usec=000000;
	                time2 = tv_sys.tv_sec*1000000+tv_sys.tv_usec; //gprmc time                                                               
                }
            }
            else{
                pps.Lock = 0;
            }          
        }
        printf("PPS interrupt value = %c\n",val);
        printf("\n LOCK status:%d\n",pps.Lock);

        local_tm=localtime(&tm_sys.tv_sec);
        printf("%d-%d-%d %d:%d:%d +%6ld\n",local_tm->tm_year+1900,local_tm->tm_mon,local_tm->tm_mday,
                                                    local_tm->tm_hour,local_tm->tm_min,local_tm->tm_sec,tm_sys.tv_usec);
        printf("diff time:%ld,now system time:%ld,last system time:%ld\n",time2-time1,time2,time1);
	}
    return 0;
}
