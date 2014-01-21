/*
DM3730-gpmc for FPGA
*/
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <plat/gpmc.h>
#include <plat/sdrc.h>
#include <asm/mach-types.h>
#include "plat/gpio.h"
#include "plat/dma.h"

#include "asm/uaccess.h"
#include "asm/io.h"
#include "asm/atomic.h"

#define USER_BUFF_SIZE (31501*3 + 100 + 1)

/* GPMC register offsets */
#define GPMC_REVISION		0x00
#define GPMC_SYSCONFIG		0x10
#define GPMC_SYSSTATUS		0x14
#define GPMC_IRQSTATUS		0x18
#define GPMC_IRQENABLE		0x1c
#define GPMC_TIMEOUT_CONTROL	0x40
#define GPMC_ERR_ADDRESS	0x44
#define GPMC_ERR_TYPE		0x48
#define GPMC_CONFIG		0x50
#define GPMC_STATUS		0x54
#define GPMC_PREFETCH_CONFIG1	0x1e0
#define GPMC_PREFETCH_CONFIG2	0x1e4
#define GPMC_PREFETCH_CONTROL	0x1ec
#define GPMC_PREFETCH_STATUS	0x1f0
#define GPMC_ECC_CONFIG		0x1f4
#define GPMC_ECC_CONTROL	0x1f8
#define GPMC_ECC_SIZE_CONFIG	0x1fc

#define GPMC_BASE_ADDR 		0x6E000000
#define GPMC_CS 		3
#define GPMC_CS0		0x60
#define GPMC_CS_SIZE		0x30	//0x6E00 0060 + (0x0000 0030 * i)

#define GPMC_MEM_START		0x00000000
#define GPMC_MEM_END		0x3FFFFFFF
#define BOOT_ROM_SPACE		0x100000	/* 1MB */

#define GPMC_CHUNK_SHIFT	24		/* 16 MB */
#define GPMC_SECTION_SHIFT	28		/* 128 MB */

#define PREFETCH_FIFOTHRESHOLD	(0x40 << 8)
#define CS_NUM_SHIFT		24
#define ENABLE_PREFETCH		(0x1 << 7)
#define DMA_MPU_MODE		2

//config1 == 00000000000000110001001000010011 
#define FPGA_GPMC_CONFIG1	0x00031213    
//config2 == 00000000000111000001110000000010
//CSontime = 2 fclk, CSRDofftime = 28 fclk, CSWROFFTIME = 28 fclk
#define FPGA_GPMC_CONFIG2	0x001C1C02
//config3 == 00000000000001000000010000000001
#define FPGA_GPMC_CONFIG3	0x00040401
//config4 == 00010000000001010001000000000101
#define FPGA_GPMC_CONFIG4	0x10051005	
//config5 == 00000001000110100001111100011111
//RDCYCLETIME = 31 fclk, WRCYCLETIME = 31fclk, RDACCESSTIME = 26 fclk,PAGEBURSTACCESSTIME = 1 fclk
#define FPGA_GPMC_CONFIG5	0x011A1F1F	
//config6 == 10001111000001010000000000000000
#define FPGA_GPMC_CONFIG6	0x8F050000
//config7 == 00000000000000000000110000110101
//BaseAddress = 110101 = 0x35000000, MASKADDRESS = 1100 = 64M
#define FPGA_GPMC_CONFIG7	0x000000C35



static struct resource  *io_mem;

struct fpga_dev {
		dev_t devt;
		struct cdev cdev;
		struct semaphore sem;
		struct class *class;
		unsigned char *user_buff;
};

static struct fpga_dev fpga_dev;
unsigned long mem_base;
static void __iomem *fpga_base;
static void __iomem *gpmc_base;

static const u32 gpmc_nor[7] = {
	FPGA_GPMC_CONFIG1,
	FPGA_GPMC_CONFIG2,
	FPGA_GPMC_CONFIG3,
	FPGA_GPMC_CONFIG4,
	FPGA_GPMC_CONFIG5,
	FPGA_GPMC_CONFIG6,
	FPGA_GPMC_CONFIG7,
};
static void gpmc_write_reg(int idx, u32 val)
{
	__raw_writel(val, gpmc_base + idx);
}
static u32 gpmc_read_reg(int idx)
{
	return __raw_readl(gpmc_base + idx);
}

static ssize_t fpga_write(struct file *filp, const char __user *buff, size_t count, loff_t *f_pos)
{
        ssize_t status;
        size_t len = USER_BUFF_SIZE - 1;
        unsigned short tmp;
	 int i, j = 0;
        if (count == 0)
                return 0;
        if (down_interruptible(&fpga_dev.sem))
                return -ERESTARTSYS;

        if (len > count)
                len = count;      

	 memset(fpga_dev.user_buff, 0, USER_BUFF_SIZE);
        if (copy_from_user(fpga_dev.user_buff, buff, len)) {
                status = -EFAULT;
                goto fpga_write_done;
        }

        /* do something with the user data */

        printk("fpga_write and count = %d. \n", count);
        for(i=0; i<count; i=i+2) {
	     if(i == (count-1)) {
	         tmp = fpga_dev.user_buff[i] | 0x00FF;            
	     } else {
	         tmp = fpga_dev.user_buff[i] | (fpga_dev.user_buff[i+1]<<8);
	     }
	     iowrite16(tmp,fpga_base+i);
	     printk("Sending data[%d] = %X\n",j, tmp);
	     j++;
        }
        
fpga_write_done:

        up(&fpga_dev.sem);

        return status;
}

static ssize_t fpga_read(struct file *filp, char __user *buff, size_t count, loff_t *offp)
{

        ssize_t status;
        size_t len;
	 unsigned short tmp = 0;
        int i ;

        if (*offp > 0)
                return 0;
        if (down_interruptible(&fpga_dev.sem))
                return -ERESTARTSYS;
		
        memset(fpga_dev.user_buff, 0, USER_BUFF_SIZE);	
        for(i=0;i<count;i = i +2) {
	     if(i == (count -1)) {
	         tmp = ioread8(fpga_base + i);
	         fpga_dev.user_buff[i] = (tmp);
	     }else {
	         tmp = ioread16(fpga_base + i);
		  fpga_dev.user_buff[i] = (tmp)&0x00FF;
	         fpga_dev.user_buff[i+1] = tmp>>8;
	     }
	     //printk("Recving data = %X\n", tmp);

        }
        
        fpga_dev.user_buff[count] = '\0';
		
        len = strlen((char*)(fpga_dev.user_buff));
        
        if (len > count)
                len = count;

        if (copy_to_user(buff, fpga_dev.user_buff, count)) {
                status = -EFAULT;
                goto fpga_read_done;
        }
   	 
fpga_read_done:
                       
        up(&fpga_dev.sem);
       
        return status;    

}

static int fpga_open(struct inode *inode, struct file *filp)
{ 
	int status = 0;
	if (down_interruptible(&fpga_dev.sem)) 
		return -ERESTARTSYS;
	if (!fpga_dev.user_buff) {
		fpga_dev.user_buff = kmalloc(USER_BUFF_SIZE, GFP_KERNEL);
		if (!fpga_dev.user_buff) {
			printk(KERN_ALERT "~o_o~fpga_open: user_buff alloc failed\n");
			status = -ENOMEM;
		}
	}
	up(&fpga_dev.sem);
	return status;
}

static int fpga_release(struct inode *inode, struct file *filp)
{
    return 0;
}

static const struct file_operations fpga_fops = {
	.owner = THIS_MODULE,
	.open = fpga_open, 
	.read = fpga_read,
	.write = fpga_write,
	.release = fpga_release,
};

static int __init fpga_init_cdev(void)
{
	int error;
	u32 val;
	int dev_major = 0;
	int dev_minor = 0;
	int dev_nr_devs = 1;
	int result;
	int i = 1;
	if(dev_major) {
	    fpga_dev.devt = MKDEV(dev_major, dev_minor);
	    result = register_chrdev_region(fpga_dev.devt, dev_nr_devs, "fpga");
	} else {
	    result = alloc_chrdev_region(&fpga_dev.devt, dev_minor, dev_nr_devs, "fpga");
	    dev_major = MAJOR(fpga_dev.devt);
	}
	if(result<0) {
	    //unregister_chrdev_region(fpga_dev.devt, 1);  //you can add this one
	    printk(KERN_WARNING "dev:can't get major %d\n", dev_major);
	} else {
	    printk(KERN_WARNING "dev:succed get major %d\n", dev_major);
	}

	cdev_init(&fpga_dev.cdev, &fpga_fops);
	fpga_dev.cdev.owner = THIS_MODULE;
	fpga_dev.cdev.ops = &fpga_fops;
	error = cdev_add(&fpga_dev.cdev, fpga_dev.devt, 1);
	if (error) {
		//cdev_del(&fpga_dev.cdev);//you can add this one
		printk(KERN_NOTICE  "Error  %d adding dev\n", error);
		return error;
	} else {
	    printk("******Succed add the device!\n");
	}
	
	printk("******Getting Chip Select\n");
	io_mem = request_region(GPMC_BASE_ADDR, SZ_4K, "io");
	if (io_mem == NULL)  
	{ 
		printk("~o_o~failed to get memory region\n"); 
	}
	gpmc_base = ioremap(GPMC_BASE_ADDR, SZ_4K);
	printk("******gpmc_base address 0x%p\n", gpmc_base);
	val = gpmc_read_reg(GPMC_REVISION);
	printk("******GPMC revision %d.%d\n", (val >> 4) & 0x0f, val & 0x0f);

        gpmc_write_reg(GPMC_IRQENABLE, 0);
        gpmc_write_reg(GPMC_TIMEOUT_CONTROL, 0);
			
	val = gpmc_read_reg(GPMC_CONFIG);
	gpmc_write_reg(GPMC_CONFIG, (val & 0xFFFFFFFD));
	gpmc_cs_write_reg(GPMC_CS, GPMC_CS_CONFIG1, gpmc_nor[0]);
	//gpmc_cs_write_reg(GPMC_CS, GPMC_CS_CONFIG2, gpmc_nor[1]);
	gpmc_cs_write_reg(GPMC_CS, GPMC_CS_CONFIG3, gpmc_nor[2]);
	gpmc_cs_write_reg(GPMC_CS, GPMC_CS_CONFIG4, gpmc_nor[3]);
	//gpmc_cs_write_reg(GPMC_CS, GPMC_CS_CONFIG5, gpmc_nor[4]);
	gpmc_cs_write_reg(GPMC_CS, GPMC_CS_CONFIG6, gpmc_nor[5]);
	gpmc_cs_write_reg(GPMC_CS, GPMC_CS_CONFIG7, gpmc_nor[6]);


	printk("******GPMC_CS3_CONFIG1 value 0x%x\n",gpmc_cs_read_reg(3, GPMC_CS_CONFIG1));
       printk("******GPMC_CS3_CONFIG2 value 0x%x\n",gpmc_cs_read_reg(3, GPMC_CS_CONFIG2));
	printk("******GPMC_CS3_CONFIG3 value 0x%x\n",gpmc_cs_read_reg(3, GPMC_CS_CONFIG3));
	printk("******GPMC_CS3_CONFIG4 value 0x%x\n",gpmc_cs_read_reg(3, GPMC_CS_CONFIG4));
	printk("******GPMC_CS3_CONFIG5 value 0x%x\n",gpmc_cs_read_reg(3, GPMC_CS_CONFIG5));
	printk("******GPMC_CS3_CONFIG6 value 0x%x\n",gpmc_cs_read_reg(3, GPMC_CS_CONFIG6));
	printk("******GPMC_CS3_CONFIG7 value 0x%x\n",gpmc_cs_read_reg(3, GPMC_CS_CONFIG7));

	
	if (gpmc_cs_request(GPMC_CS, SZ_2K, (unsigned long *)&mem_base) < 0){ 
		printk(KERN_ERR "~o_o~Failed request for GPMC mem for usrp_e\n");
		return -1;
	}
	printk("******Get CS%d address = %lx\n", GPMC_CS,mem_base);
	printk("******Get mem fpga address = %p\n", request_mem_region(mem_base, SZ_2K, "mem_fpga"));
	fpga_base = ioremap(mem_base, SZ_2K); 
	printk("******Get fpga address = %p\n", fpga_base);
	return 0;
}

static int __init fpga_init_class(void)
{
	struct device *device;
	fpga_dev.class = class_create(THIS_MODULE, "fpga");
	if (IS_ERR(fpga_dev.class)) {
		printk(KERN_ALERT "~o_o~class_create(fpga) failed\n");
		return PTR_ERR(fpga_dev.class);
	}
	device = device_create(fpga_dev.class, NULL, fpga_dev.devt, NULL,"fpga");
	if (IS_ERR(device)) {
		class_destroy(fpga_dev.class);
		return PTR_ERR(device);
	}
		return 0;
}

static int __init fpga_init(void)
{
	printk(KERN_INFO "**fpga_init**\n");
	memset(&fpga_dev, 0, sizeof(struct fpga_dev));
	sema_init(&fpga_dev.sem, 1);
	if (fpga_init_cdev())
		goto init_fail_1;
	if (fpga_init_class())
		goto init_fail_2;
	return 0;
	init_fail_2:
	cdev_del(&fpga_dev.cdev);
	unregister_chrdev_region(fpga_dev.devt, 1);
	init_fail_1:
	return -1;
}
module_init(fpga_init);

static void __exit fpga_exit(void)
{
	printk(KERN_INFO "**fpga_exit**\n");
	device_destroy(fpga_dev.class, fpga_dev.devt);
	class_destroy(fpga_dev.class);
	cdev_del(&fpga_dev.cdev);
	unregister_chrdev_region(fpga_dev.devt, 1);
	release_mem_region(mem_base, SZ_2K);
	release_region(GPMC_BASE_ADDR, SZ_4K);
	gpmc_cs_free(GPMC_CS);
	iounmap(fpga_base);
	if (fpga_dev.user_buff)
		kfree(fpga_dev.user_buff);
}
module_exit(fpga_exit);


MODULE_AUTHOR("s_r_h");
MODULE_DESCRIPTION("fpga driver");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_VERSION("0.1");

