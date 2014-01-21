#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define DEV_X86 "/mnt/hgfs/vm-shared/gpmc_fpga/usr/iEM.bin"

int main(void)
{
    int buf[1024]={0,0};
    int i = 0;
    FILE *fd = NULL;
    if((fd=fopen(DEV_X86,"w"))==NULL) {
        perror("Fault open the DEV_X86.");
    }
    for(i = 0; i<(sizeof(buf)/sizeof(int)); i++) {
	buf[i] = i;
    }
    for(i = 0; i<(sizeof(buf)/sizeof(int))-1; i++) {
	printf("buf[%d] =%d..\n ",i,buf[i]);
    }
    printf("sizeof(buf) = %d,sizeof(buf)/sizeof(int)= %d..\n",sizeof(buf),sizeof(buf)/sizeof(int));
    fwrite(buf,sizeof(int),sizeof(buf)/sizeof(int),fd);
    fclose(fd);
    return 0;
}


