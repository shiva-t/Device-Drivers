#include<linux/kernel.h>
#include<linux/blkdev.h>
#include<linux/bio.h>
#include<linux/genhd.h>
#include<linux/types.h>
#include<linux/init.h>
#include<linux/module.h>
#include<linux/fs.h>
#include<linux/errno.h>
#include<linux/uaccess.h>
#include<linux/sched.h>
#include<linux/workqueue.h>
#include<linux/usb.h>

#define MS_VID 0xC251
#define MS_PID 0x1303
#define FIRST_MINOR 1
#define DEV_NAME "LPC_FLASH"
#define MINORS 1
#define DEV_NR_SEC 1024
#define DEV_SEC_SIZE 512
#define KERNEL_SEC_SIZE 512

#pragma pack(1)

/* Bulk-only Command Block Wrapper */
typedef struct _MSC_CBW {
  uint32_t dSignature;
  uint32_t dTag;
  uint32_t dDataLength;
  uint8_t  bmFlags;
  uint8_t  bLUN;
  uint8_t  bCBLength;
  uint8_t  CB[16];
} MSC_CBW;

/* Bulk-only Command Status Wrapper */
typedef struct _MSC_CSW {
  uint32_t dSignature;
  uint32_t dTag;
  uint32_t dDataResidue;
  uint8_t  bStatus;
} MSC_CSW;

#pragma pack()

#define MSC_CBW_Signature               0x43425355
#define MSC_CSW_Signature               0x53425355


/* CSW Status Definitions */
#define CSW_CMD_PASSED                  0x00
#define CSW_CMD_FAILED                  0x01
#define CSW_PHASE_ERROR                 0x02


/* SCSI Commands */
#define SCSI_TEST_UNIT_READY            0x00
#define SCSI_REQUEST_SENSE              0x03
#define SCSI_FORMAT_UNIT                0x04
#define SCSI_INQUIRY                    0x12
#define SCSI_MODE_SELECT6               0x15
#define SCSI_MODE_SENSE6                0x1A
#define SCSI_START_STOP_UNIT            0x1B
#define SCSI_MEDIA_REMOVAL              0x1E
#define SCSI_READ_FORMAT_CAPACITIES     0x23
#define SCSI_READ_CAPACITY              0x25
#define SCSI_READ10                     0x28
#define SCSI_WRITE10                    0x2A
#define SCSI_VERIFY10                   0x2F
#define SCSI_MODE_SELECT10              0x55
#define SCSI_MODE_SENSE10               0x5A

/* STATES OF THE DRIVER*/
#define SEND_CBW_OUT                    0
#define SEND_OUT_WRITE                  1
#define SEND_IN_READ                    2
#define SEND_IN_CSW                     3
#define CHECK_STATUS_TRANS              4


static int dev_connect(struct usb_interface* intf,const struct usb_device_id *id);
static void dev_disconnect(struct usb_interface* intf);
void block_request(struct request_queue* que);
void urb_callback(struct urb* urb);

/*------------------------------------------------------------------*/
int major;

unsigned int* status = NULL;

MSC_CBW* CBW = NULL;
MSC_CSW* CSW = NULL;

struct usb_device* udev = NULL;

struct urb* urb = NULL;
struct urb* urb_k = NULL;
unsigned int pipe;

sector_t start_sector;
sector_t xfer_sectors;
unsigned char* buffer = NULL;
unsigned int offset;
size_t xfer_len;

struct block_device_operations my_block_ops ={
  .owner = THIS_MODULE
};
struct my_block_struct
{
  spinlock_t lock;
  struct gendisk* gd;
  struct request_queue* queue;
};

struct my_block_struct* device = NULL;

static struct private_usbdev
{
  struct usb_device* u_dev;
};

static const struct usb_device_id usb_dev_id_table[] = {
  {USB_DEVICE(MS_VID,MS_PID)},
  {}
};

static struct private_usbdev* p_usbdev = NULL;

static struct usb_driver usbdev_driver = {
  name:"Mass storage Driver",
  probe:dev_connect,
  disconnect:dev_disconnect,
  id_table:usb_dev_id_table,
} ;
/*------------------------------------------------------------------*/
void urb_callback(struct urb* urb_c)
{
  unsigned int* driver_status = (urb_c->context);
  int res_callback;
  if(!(urb_c->status))
  {
    switch (*driver_status) {
      case SEND_OUT_WRITE:
              printk(KERN_INFO"CBW - out request successful\n");
              usb_free_urb(urb_c);
              kfree(CBW);
              urb_k = usb_alloc_urb(0,GFP_ATOMIC);
              pipe = usb_sndbulkpipe(udev,2);
              usb_fill_bulk_urb(urb_k,udev,pipe,buffer+offset,xfer_len,urb_callback,driver_status);
//              urb_k->transfer_flags = URB_ASYNC_UNLINK;
              res_callback = usb_submit_urb(urb_k,GFP_ATOMIC);
              if(res_callback)
              {
                printk(KERN_INFO"Write-OUT URB Submit failed\n");
                printk(KERN_INFO"Transaction failed...\n");
                kfree(driver_status);
                usb_free_urb(urb_k);
                break;
              }
              printk(KERN_INFO"Write-OUT URB Submit is successful\n");
              *driver_status = SEND_IN_CSW;
              break;
      case SEND_IN_READ:
              printk(KERN_INFO"CBW - out request successful\n");
              usb_free_urb(urb_c);
              kfree(CBW);
              urb_k = usb_alloc_urb(0,GFP_ATOMIC);
              pipe = usb_rcvbulkpipe(udev,2);
              usb_fill_bulk_urb(urb_k,udev,pipe,buffer+offset,xfer_len,urb_callback,driver_status);
//              urb_k->transfer_flags = URB_ASYNC_UNLINK;
              res_callback = usb_submit_urb(urb_k,GFP_ATOMIC);
              if(res_callback)
                {
                 printk(KERN_INFO"Read-IN URB Submit failed\n");
                 printk(KERN_INFO"Transaction failed...\n");
                 usb_free_urb(urb_k);
                 kfree(driver_status);
                 break;
                }
                printk(KERN_INFO"Read-IN URB Submit is successful\n");
                *driver_status = SEND_IN_CSW;
                break;
      case SEND_IN_CSW:
              printk(KERN_INFO"READ/WRITE request successful\n");
              usb_free_urb(urb_c);
              CSW = kmalloc(sizeof(MSC_CSW),GFP_ATOMIC);
              urb_k = usb_alloc_urb(0,GFP_ATOMIC);
              pipe = usb_rcvbulkpipe(udev,2);
              usb_fill_bulk_urb(urb_k,udev,pipe,CSW,sizeof(MSC_CSW),urb_callback,driver_status);
//              urb_k->transfer_flags = URB_ASYNC_UNLINK;
              res_callback = usb_submit_urb(urb_k,GFP_ATOMIC);
              if(res_callback)
                {
                  printk(KERN_INFO"CSW-IN URB Submit failed\n");
                  printk(KERN_INFO"Transaction failed...\n");
                  usb_free_urb(urb_k);
                  kfree(driver_status);
                  kfree(CSW);
                  break;
                }
                printk(KERN_INFO"CSW-IN URB Submit is successful\n");
                *driver_status = CHECK_STATUS_TRANS;
                break;
      case CHECK_STATUS_TRANS:
                usb_free_urb(urb_c);
                if(CSW->bStatus == 0){
                  printk(KERN_INFO"Transaction is successful\n");
                }
                else{
                  printk(KERN_INFO"Transaction failed in final\n");
                }
                kfree(CSW);
                printk(KERN_INFO"BP1\n");
                kfree(driver_status);
                printk(KERN_INFO"BP2\n");
                break;
    }
  }
  else
  {
    switch(*driver_status){
      case SEND_OUT_WRITE:
           printk(KERN_INFO"CBW - request failed\n");
           //usb_kill_urb(urb);
           //usb_kill_urb(urb_c);
           kfree(driver_status);
           kfree(CBW);
           break;
      case SEND_IN_READ:
           printk(KERN_INFO"CBW - request failed\n");
           //usb_kill_urb(urb);
           //usb_kill_urb(urb_c);
           kfree(driver_status);
           kfree(CBW);
           break;
      case SEND_IN_CSW:
           printk(KERN_INFO"Read/Write request failed\n");
           //usb_kill_urb(urb_c);
           usb_kill_urb(urb_k);
           kfree(driver_status);
           break;
      case CHECK_STATUS_TRANS:
           printk(KERN_INFO"CSW-IN request failed\n");
           //usb_kill_urb(urb_c);
           //usb_kill_urb(urb_k);
           kfree(driver_status);
           kfree(CSW);
           break;
    }
  }
}
/*------------------------------------------------------------------*/
void block_request(struct request_queue* que)
{
  struct request* rq;
  struct req_iterator iter;
  struct bio_vec bvec;
  rq = blk_fetch_request(que);
  while(rq != NULL)
  {
    if(blk_rq_is_passthrough(rq))
    {
			printk(KERN_INFO "non FS request\n");
			__blk_end_request_all(rq, -EIO);
			continue;
		}
    else
    {
      rq_for_each_segment(bvec,rq,iter)
      {
        int result;
        start_sector = iter.iter.bi_sector + 1;
        buffer = page_address(bvec.bv_page);
        offset = bvec.bv_offset;
        xfer_len = bvec.bv_len;
        xfer_sectors = xfer_len/DEV_SEC_SIZE;
        int dir = rq_data_dir(rq);
        CBW = kmalloc(sizeof(MSC_CBW),GFP_ATOMIC);
        urb = usb_alloc_urb(0,GFP_ATOMIC);
        status = kmalloc(sizeof(unsigned int),GFP_ATOMIC);
        CBW->dSignature = MSC_CBW_Signature;
        CBW->bLUN = 0x00;
        CBW->bCBLength = 0x10;
        CBW->CB[2] = (start_sector >> 24) & 0xFF ;
        CBW->CB[3] = (start_sector >> 16) & 0xFF ;
        CBW->CB[4] = (start_sector >> 8) & 0xFF ;
        CBW->CB[5] = (start_sector >> 0) & 0xFF ;
        CBW->CB[7] = (xfer_sectors >> 8) & 0xFF ;
        CBW->CB[8] = (xfer_sectors >> 0) & 0xFF ;
        //Write request
        if(dir == 1)
        {
          pipe = usb_sndbulkpipe(udev,2);
          CBW->dTag = 0x0005;
          CBW->dDataLength = xfer_len;
          CBW->bmFlags = 0x00;
          CBW->CB[0] = SCSI_WRITE10;
          usb_fill_bulk_urb(urb,udev,pipe,CBW,sizeof(MSC_CBW),urb_callback,status);
//          urb->transfer_flags = URB_ASYNC_UNLINK;
          result = usb_submit_urb(urb,GFP_ATOMIC);
          if(result)
          {
            printk("Write URB Submit is failed\n");
          }
          printk(KERN_INFO"Write URB Submitted Successfully\n");
          *status = SEND_OUT_WRITE;
        }
        //Read request
        else
        {
          pipe = usb_rcvbulkpipe(udev,2);
          CBW->dTag = 0x0006;
          CBW->dDataLength = xfer_len;
          CBW->bmFlags = 0x80;
          CBW->CB[0] = SCSI_READ10;
          usb_fill_bulk_urb(urb,udev,pipe,CBW,sizeof(MSC_CBW),urb_callback,status);
          result = usb_submit_urb(urb,GFP_ATOMIC);
          if(result)
          {
            printk("Read URB Submit is failed\n");
          }
          printk(KERN_INFO"Read URB Submitted Successfully\n");
          *status = SEND_IN_READ;
        }
      }
    }
    if(!__blk_end_request_cur(rq,0))
    {
      rq = blk_fetch_request(que);
    }
  }
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------
static int dev_connect(struct usb_interface* intf,const struct usb_device_id *id)
{
  udev = interface_to_usbdev(intf);
  printk(KERN_INFO"Device Connected\n");
  device = kmalloc(sizeof(struct my_block_struct),GFP_KERNEL);
  if( (major=register_blkdev(0,DEV_NAME))<0)
  {
    printk(KERN_INFO"Unable to register block device : LPC_FLASH\n");
    return -EBUSY;
  }
  printk("Major_number:%d\n",major);
  device->gd = alloc_disk(MINORS);
  if(!device->gd)
  {
    printk(KERN_INFO"Gendisk is not allocated\n");
    unregister_blkdev(major,DEV_NAME);
    kfree(device);
    return -ENOMEM;
  }
  strcpy(device->gd->disk_name,DEV_NAME);
  device->gd->first_minor = FIRST_MINOR;
  device->gd->major = major;
  device->gd->fops = &my_block_ops;
  spin_lock_init(&device->lock);
  if(!(device->gd->queue = blk_init_queue(block_request,&device->lock)))
  {
    printk("Request_queue allocated failed\n");
    del_gendisk(device->gd);
    unregister_blkdev(major,DEV_NAME);
    kfree(device);
    return -ENOMEM;
  }
  blk_queue_logical_block_size(device->gd->queue,DEV_SEC_SIZE);
  device->queue = device->gd->queue;
//  device->gd->queue->queuedata = path;
  set_capacity(device->gd,(DEV_NR_SEC*DEV_SEC_SIZE/KERNEL_SEC_SIZE));
  device->gd->private_data = device;
  add_disk(device->gd);
  printk(KERN_INFO"Block device successfully registered\n");
  return 0;
}

static void dev_disconnect(struct usb_interface* intf)
{
  blk_cleanup_queue(device->queue);
  del_gendisk(device->gd);
  unregister_blkdev(major,DEV_NAME);
  kfree(device);
  printk(KERN_INFO"Block device unregistered successfully\n");
  printk(KERN_INFO"Device disconnected\n");
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------
static int __init usb_init(void)
{
  int result;
  result = usb_register(&usbdev_driver);
  if(result)
   {
       printk(KERN_INFO"Device Registration failed\n");
       return -ENOMEM;
   }
  printk(KERN_INFO"Driver Registered\n");
  return 0;
}

static void __exit usb_exit(void)
{
  if(urb != NULL)
  {
    usb_unlink_urb(urb);
  }
  kfree(status);
  usb_deregister(&usbdev_driver);
  printk(KERN_INFO"Driver Unregistered \n");
}

module_init(usb_init);
module_exit(usb_exit);

MODULE_AUTHOR("EEEG547");
MODULE_DESCRIPTION("USB device driver");
MODULE_LICENSE("GPL");
