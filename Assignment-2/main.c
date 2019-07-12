#include <linux/vmalloc.h>
#include <linux/module.h>  
#include <linux/kernel.h>  
#include <linux/fs.h>      
#include <asm/uaccess.h>  
#include <linux/types.h>
#include <linux/genhd.h>
#include <linux/blkdev.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/version.h>
#include <linux/hdreg.h> 
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>

#define DEVICE_NAME "dof"
#define NR_OF_SECTORS 1024
#define SECTOR_SIZE 512
#define DOF_MINOR_CNT 2

int err =0;

struct my_work{
	struct work_struct work;
	sector_t sector_off;
	u8 *buffer;
	unsigned int sectors;
}my_work;


struct my_work2{
        struct work_struct work;
        sector_t sector_off;
        u8 *buffer;
        unsigned int sectors;
}my_work2;

typedef struct //structure for partition table
{
    unsigned char boot_type; // 0x00 - Inactive; 0x80 - Active (Bootable)
    unsigned char start_head;
    unsigned char start_sec:6;
    unsigned char start_cyl_hi:2;
    unsigned char start_cyl;
    unsigned char part_type;
    unsigned char end_head;
    unsigned char end_sec:6;
    unsigned char end_cyl_hi:2;
    unsigned char end_cyl;
    unsigned long abs_start_sec;
    unsigned long sec_in_part;
} PartEntry;

typedef PartEntry PartTable[4];

static PartTable def_part_table =
{
    {
        boot_type: 0x00,
        start_head: 0x00,
        start_sec: 0x2,
        start_cyl: 0x00,
        part_type: 0x83,
        end_head: 0x00,
        end_sec: 0x20,
        end_cyl: 0x09,
        abs_start_sec: 0x00000001,
        sec_in_part: 0x00000200
    },
    {
	boot_type: 0x00,
        start_head: 0x00,
        start_sec: 0x1,
        start_cyl: 0x14,
        part_type: 0x83,
        end_head: 0x00,
        end_sec: 0x20,
        end_cyl: 0x1F,
        abs_start_sec: 0x00000280,
        sec_in_part: 0x00000200
    },
    {
     
    },
    {

    }
};

void write_mbr(void)
{
	struct file *f;
	unsigned char buf[128], buf1[1024];
	unsigned char buf2[512];
	unsigned char sign[2];
	int i,j=0;
	for(i=0;i<512;i++) { buf2[i]=0; }
	
	for(i=0;i<=1;i++){
	buf[j++]=def_part_table[i].boot_type  ;
    	buf[j++]=def_part_table[i].start_head;
    	buf[j++]=def_part_table[i].start_sec;
    	buf[j++]=def_part_table[i].start_cyl;
    	buf[j++]=def_part_table[i].part_type;
    	buf[j++]=def_part_table[i].end_head;
    	buf[j++]=def_part_table[i].end_sec;
    	buf[j++]=def_part_table[i].end_cyl;
	buf[j++]=( ((def_part_table[i].abs_start_sec) & (0x000000FF))) ;
	buf[j++]=( ((def_part_table[i].abs_start_sec) & (0x0000FF00)) >> 8) ;
	buf[j++]=( ((def_part_table[i].abs_start_sec) & (0x00FF0000)) >> 16) ;
	buf[j++]=( ((def_part_table[i].abs_start_sec) & (0xFF000000)) >> 24) ;
	buf[j++]=( ((def_part_table[i].sec_in_part) & (0x000000FF))) ;
	buf[j++]=( ((def_part_table[i].sec_in_part) & (0x0000FF00)) >> 8) ;
	buf[j++]=( ((def_part_table[i].sec_in_part) & (0x00FF0000)) >> 16) ;
	buf[j++]= ( ((def_part_table[i].sec_in_part) & (0xFF000000)) >> 24) ;
	}
	sign[0]=(0x55);
	sign[1]=(0xAA);	

	printk(KERN_ALERT "FILERW DEMO Module\n");	

	f=filp_open("/etc/sample.txt",O_RDWR,0666);
	if(f==NULL)
		printk(KERN_INFO "Error in file opening\n");
	else {
		printk("%d\n",kernel_write(f,buf2,446,0));
		printk("%d\n",kernel_write(f,buf,32,446));
		printk("%d\n",kernel_write(f,buf2,32,478));
		printk("%d\n",kernel_write(f,sign,2,510)); 
		printk("%d\n",kernel_read(f,0,buf1,512));
		//for(i=0;i<512;i++) printk("%x ",*(buf1+i));
		}
	filp_close(f,NULL);

	return;
}

struct blkdev_private{  //private structure
	struct request_queue *queue;  
	struct gendisk *gd;
	spinlock_t lock;
};

struct request *req;

static int dof_open(struct block_device *bdev, fmode_t mode)
{
        unsigned unit = iminor(bdev->bd_inode);

        printk(KERN_INFO "dof: Device is opened\n");
        printk(KERN_INFO "dof: Inode number is %d\n", unit);

        if (unit > DOF_MINOR_CNT)
                return -ENODEV;
        return 0;
}

static void dof_close(struct gendisk *disk, fmode_t mode)
{
        printk(KERN_INFO "dof: Device is closed\n");
}

static struct block_device_operations blkdev_ops =
{
        .owner = THIS_MODULE,
        .open = dof_open,
        .release = dof_close,
};

static struct blkdev_private *p_blkdev = NULL;

void dof_write(sector_t sector_off, u8 *buffer, unsigned int sectors)
{
	struct file *f;
	f=filp_open("/etc/sample.txt",O_RDWR,0666);
	if(f==NULL)
                printk(KERN_INFO "Error in file opening\n");
        else {
                printk("%d\n",kernel_write(f,buffer,sectors * SECTOR_SIZE, sector_off * SECTOR_SIZE));
                }
        filp_close(f,NULL);
}

void dof_read(sector_t sector_off, u8 *buffer, unsigned int sectors)
{
	struct file *f;
        f=filp_open("/etc/sample.txt",O_RDWR,0666);
        if(f==NULL)
                printk(KERN_INFO "Error in file opening\n");
        else {
                printk("%d\n",kernel_read(f, sector_off * SECTOR_SIZE,buffer,sectors * SECTOR_SIZE));
                }
        filp_close(f,NULL);
}

static void write_bh(struct work_struct *work)
{
        struct my_work2 *mwp=container_of(work,struct my_work2,work);//gives pointer to pvt str
        sector_t sector_off=mwp->sector_off;
        u8 *buffer=mwp->buffer;
        unsigned int sectors=mwp->sectors;
        printk("dof:write Defered function executed\n");
        dof_write(sector_off, buffer, sectors);
	return ;
}

static void read_bh(struct work_struct *work)
{
        struct my_work *mwp=container_of(work,struct my_work,work);//gives pointer to pvt str
        sector_t sector_off=mwp->sector_off;
        u8 *buffer=mwp->buffer;
        unsigned int sectors=mwp->sectors;
        printk("dof:read Defered function executed\n");
        dof_read(sector_off, buffer, sectors);
        return ;
}


static int dof_transfer(struct request *req)
{
	int dir = rq_data_dir(req);
	sector_t start_sector = blk_rq_pos(req);
	unsigned int sector_cnt = blk_rq_sectors(req);

	#define BV_PAGE(bv) ((bv).bv_page)
	#define BV_OFFSET(bv) ((bv).bv_offset)
	#define BV_LEN(bv) ((bv).bv_len)
	struct bio_vec bv;

	struct req_iterator iter;

	sector_t sector_offset;
	unsigned int sectors;
	u8 *buffer;

	int ret = 0;

	sector_offset = 0;
	rq_for_each_segment(bv, req, iter)
	{
		buffer = page_address(BV_PAGE(bv)) + BV_OFFSET(bv);
		if (BV_LEN(bv) % SECTOR_SIZE != 0)
		{
			printk(KERN_ERR "dof: Should never happen: "
				"bio size (%d) is not a multiple of SECTOR_SIZE (%d).\n"
				"This may lead to data truncation.\n",
				BV_LEN(bv), SECTOR_SIZE);
			ret = -EIO;
		}
		sectors = BV_LEN(bv) / SECTOR_SIZE;
		printk(KERN_DEBUG "dof: Start Sector: %llu, Sector Offset: %llu; Buffer: %p; Length: %u sectors\n",
			(unsigned long long)(start_sector), (unsigned long long)(sector_offset), buffer, sectors);
		if (dir == WRITE) /* Write to the device */
		{
			printk("dof: Writing...\n");
			my_work2.sector_off=start_sector+sector_offset;
                        my_work2.buffer=buffer;
                        my_work2.sectors=sectors;
                        schedule_work(&my_work2.work);
		}
		else /* Read from the device */
		{
			printk("dof: Reading...\n");
			my_work.sector_off=start_sector+sector_offset;
			my_work.buffer=buffer;
			my_work.sectors=sectors;
			schedule_work(&my_work.work);
			//dof_read(start_sector + sector_offset, buffer, sectors);
		}
		sector_offset += sectors;
	}
	if (sector_offset != sector_cnt)
	{
		printk(KERN_ERR "dof: bio information doesn't match the request info");
		ret = -EIO;
	}
	return ret;
}

static void dof_request(struct request_queue *q)
{
	struct request *req;
	int ret;

	/* Gets the current request from the dispatch queue */
	while ((req = blk_fetch_request(q)) != NULL)
	{
#if 0
		if (!blk_fs_request(req))
		{
			printk(KERN_NOTICE "rb: Skip non-fs request\n");
			/* We pass 0 to indicate that we successfully completed the request */
			__blk_end_request_all(req, 0);
			//__blk_end_request(req, 0, blk_rq_bytes(req));
			continue;
		}
#endif
		ret = dof_transfer(req);
		__blk_end_request_all(req, ret);
		//__blk_end_request(req, ret, blk_rq_bytes(req));
	}
}

static int block_init(void)
{
	INIT_WORK(&my_work.work,read_bh);
	INIT_WORK(&my_work2.work,write_bh);

	write_mbr();

	struct gendisk *fod_disk = NULL;//gendisk struct pointer initialised to NULL

	err = register_blkdev(0, "Fileon DISK"); 
	if (err < 0) 
		printk(KERN_WARNING "dof: unable to get major number\n");
	
	/*allocating memory to private structure*/
	p_blkdev = kmalloc(sizeof(struct blkdev_private),GFP_KERNEL);
	
	if(!p_blkdev)
	{
		printk("ENOMEM  at %d\n",__LINE__);
		return 0;
	}
	//initialising memmory with 0's
	memset(p_blkdev, 0, sizeof(struct blkdev_private));

	spin_lock_init(&p_blkdev->lock);//hold spinlock

	p_blkdev->queue = blk_init_queue(dof_request, &p_blkdev->lock);//pass request function of driver,second is spin lock

	//gendifk struct for device, so that all info about device can be filled into it,dne using alloc_disk,returns a gendisk struct
	fod_disk = p_blkdev->gd = alloc_disk(2);
	if(!fod_disk)
	{
		kfree(p_blkdev);
		printk(KERN_INFO "alloc_disk failed\n");
		return 0;
	}
	
	//filling the gendisk struct with disk information
	fod_disk->major = err;
	fod_disk->first_minor = 0;
	fod_disk->fops = &blkdev_ops;
	fod_disk->queue = p_blkdev->queue;
	fod_disk->private_data = p_blkdev;
	strcpy(fod_disk->disk_name, DEVICE_NAME);
	set_capacity(fod_disk,NR_OF_SECTORS );
	add_disk(fod_disk); 
	printk(KERN_INFO "Registered disk\n");
	
	return 0;	
}

static void block_exit(void)
{
	struct gendisk *fod_disk = p_blkdev->gd;
	del_gendisk(fod_disk);
	blk_cleanup_queue(p_blkdev->queue);
	kfree(p_blkdev);
	printk("dof: Exiting\n");
	return;
}


module_init(block_init);
module_exit(block_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Shiva");
MODULE_DESCRIPTION("Disk on File");
