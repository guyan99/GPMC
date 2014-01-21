#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#define GPIO_13_DIR "/sys/class/gpio/gpio13/value"
#define GPIO_186_DIR "/sys/class/gpio/gpio186/value"

#define FPGA_DEV  "/dev/fpga"
#define PAGE_SIZE 254
#define USER_BUFF 254
//#define base_address 0xe1fd4000
static int io_10 = -1;//data irq
static int io_13 = -1;//send finish
static int io_186 = -1; // write-1 or read-0
unsigned char io186_value = '0';
unsigned char io13_value = '0';
int io_ret = -1;

int main(void)
{  
    int fd,i,res,j;  
    int ret = 0;
    unsigned char buf[PAGE_SIZE];   
    unsigned char user_buff[USER_BUFF];
	
    printf("GPMC Test version 1.0-BeagleBone Build on %s %s\n\r",__DATE__,__TIME__);  
    fd=open(FPGA_DEV,O_RDWR);  
    if(fd<0)  {    
        printf("Can't Open %s !!!\n\r",FPGA_DEV);    
        return -1;  
    }   

    io_13 = open(GPIO_13_DIR,O_RDWR);
    if(io_13 < 0) {
        perror("failed to open the gpio13 file.\n");
	 //return -1;
    }
    ret = write(io_13, &io13_value, sizeof(io13_value));

    io186_value = '1';
    io_186 = open(GPIO_186_DIR,O_RDWR);
    if(io_186 < 0) {
        perror("failed to open the gpio186 file.\n");
	 //return -1;
    }
    ret = write(io_186, &io186_value, sizeof(io186_value));
	



	
    for(i=0;i<PAGE_SIZE;i++)  {    
        buf[i] = i;  
    }  
    memset(user_buff, 0, sizeof(user_buff));
    while (1) {
        usleep(100000);
		
	  io186_value = '0';
        io_ret = write(io_186, &io186_value, sizeof(io186_value));		
	 io13_value = '0';
        io_ret = write(io_13, &io13_value, sizeof(io13_value));	
	 read(fd,user_buff,USER_BUFF);
	 
	  usleep(100000);
	  
	  io186_value = '1';
        io_ret = write(io_186, &io186_value, sizeof(io186_value));	
	 io13_value = '1';
        io_ret = write(io_13, &io13_value, sizeof(io13_value));	
        write(fd,buf,PAGE_SIZE);	

    }
	
    usleep(100000);
    close(fd);  
    return 0;
}
