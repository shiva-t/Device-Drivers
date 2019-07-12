#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kdev_t.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/random.h>
#include <linux/slab.h>
#include <linux/errno.h>

static dev_t dev_no;
static struct class *cls;
static struct cdev c_dev;

int i=0;

int *buf_ent;

static int my_open (struct inode *pinode, struct file *pfile){
	printk(KERN_INFO "Inside ADXL: my_open\n");
	return 0;
}

static int my_close (struct inode *pinode, struct file *pfile){
	printk(KERN_INFO "Inside ADXL: my_close\n");
	return 0;
}

static ssize_t my_read (struct file *pfile, char __user *pbuffer, size_t len, loff_t *offset){
	char *p;
	struct file *fp;
	p=kmalloc(4,GFP_KERNEL);
	buf_ent=(int*)p;
	printk(KERN_INFO "Inside ADXL: my_read\n");
	fp=filp_open("/dev/random",O_NONBLOCK,0);
	if(fp==NULL){
		printk(KERN_INFO "/dev/random file could not be opened\n");
	return -EFAULT;
	}
	
	if(!(fp->f_op->read(fp,p,4,&fp->f_pos))){
		printk(KERN_INFO "/dev/random file could not be opened");
	return -EFAULT;
	}

	*buf_ent=*buf_ent & (0x03ff);  //masking 10 bits
	if(copy_to_user(pbuffer, buf_ent,4)) printk(KERN_INFO "All data could not be sent\n");
	return 0;
}

static struct file_operations fops={
	.owner=THIS_MODULE,
	.open=my_open,
	.release=my_close,
	.read=my_read,
};

static int __init ADXL_init(void){
	printk(KERN_INFO "Entered function:%s \n",__FUNCTION__);

	/*allocate device number*/
	if(alloc_chrdev_region(&dev_no,0,3,"driver_ADXL")<0){
		printk(KERN_INFO "Allocation of device number failed\n");
		return -1;
	 } else 	printk(KERN_INFO "The major and minor numbers allocated are : %d, %d\n",MAJOR(dev_no),MINOR(dev_no));

	/*create device file*/
	if((cls=class_create(THIS_MODULE,"chardev"))==NULL){
		printk(KERN_INFO "class create failed\n");
		unregister_chrdev_region(dev_no,1);
		return -1;
	}

	/*creating three device files for x,y,z axis*/ 
	if(device_create(cls, NULL, MKDEV(MAJOR(dev_no), MINOR(dev_no) + 0), NULL, "adxl_x")==NULL){
                class_destroy(cls);
                printk(KERN_INFO "adxl_x device_create failed\n");
                unregister_chrdev_region(dev_no,1);
                return -1;
        }	

	if(device_create(cls, NULL, MKDEV(MAJOR(dev_no), MINOR(dev_no) + 1), NULL, "adxl_y")==NULL){
                class_destroy(cls);
                printk(KERN_INFO "adxl_y device_create failed\n");
                unregister_chrdev_region(dev_no,2);
                return -1;
        }	

	if(device_create(cls, NULL, MKDEV(MAJOR(dev_no), MINOR(dev_no) + 2), NULL, "adxl_z")==NULL){
                class_destroy(cls);
                printk(KERN_INFO "adxl_z device_create failed\n");
                unregister_chrdev_region(dev_no,3);
                return -1;
        }

	/*cdev structure*/
	cdev_init(&c_dev,&fops);
        if(cdev_add(&c_dev,dev_no,3)==-1){
                for(i=0;i<3;i++){
	        device_destroy(cls, MKDEV(MAJOR(dev_no), MINOR(dev_no) + i));}
		class_destroy(cls);
                unregister_chrdev_region(dev_no,1);
        }

	return 0;
}

static void __exit ADXL_exit(void){
	cdev_del(&c_dev);
	
	for(i=0;i<3;i++){
        device_destroy(cls, MKDEV(MAJOR(dev_no), MINOR(dev_no) + i));}
	
	class_destroy(cls);
	unregister_chrdev_region(dev_no,3);
	printk(KERN_INFO "Exiting from function:%s\n",__FUNCTION__);
}

module_init(ADXL_init);
module_exit(ADXL_exit);

MODULE_AUTHOR("Shiva Tripathi <h20140401@pilani.bits-pilani.ac.in>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("EEE G547 Assignment-1");
