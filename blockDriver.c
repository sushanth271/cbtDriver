#include<linux/init.h>
#include<linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include<linux/fs.h>
#include <linux/genhd.h>
#include <linux/blkdev.h>
#include <linux/bio.h>
#include <linux/vmalloc.h>
#include <linux/hdreg.h>

#define KERNEL_SECTOR_SIZE 512
int nsectors = 1024;
int hardsect_size = 512;
int major_num = 0;
static struct request_queue *Queue;

struct sbull_dev{
	int size;
	u8 *data;
	spinlock_t lock;
	struct request_queue *queue;
	struct gendisk *gd;	
}blockDevice;

void block_driver_exit(void)
{
	del_gendisk(blockDevice.gd);
    put_disk(blockDevice.gd);
    unregister_blkdev(major_num, "bd");
    blk_cleanup_queue(Queue);
    vfree(blockDevice.data);
}

void bd_transfer(struct sbull_dev *device, unsigned long sector, unsigned long nsect, char *buffer, int write)
{
	unsigned long offset = sector*KERNEL_SECTOR_SIZE;
	unsigned long nbytes = nsect*KERNEL_SECTOR_SIZE;

	if ((offset + nbytes) > device->size) {
		printk (KERN_NOTICE "Beyond-end write (%ld %ld)\n", offset, nbytes);
		return;
	}
	if (write)
		memcpy(device->data + offset, buffer, nbytes);
	else
		memcpy(buffer, device->data + offset, nbytes);	
}

static void bd_request(struct request_queue_t *q)
{
	struct request *req;

	while ((req = elv_next_request(q)) != NULL) {
		if (! blk_fs_request(req)) {
			printk (KERN_NOTICE "Error");
			end_request(req, 0);
			continue;
		}
	bd_transfer(&blockDevice, req->sector, req->current_nr_sectors, req->buffer, rq_data_dir(req));
	end_request(req, 1);
    }
}


int block_driver_entry(void)
{
	blockDevice.size = nsectors * hardsect_size;
	spin_lock_init(&blockDevice.lock);
	blockDevice.data = vmalloc(blockDevice.size);
	Queue = blk_init_queue(bd_request, &blockDevice.lock);
	major_num = register_blkdev(major_num, "bd");
	//set gendisk str
	blockDevice.gd->major = major_num;
	blockDevice.gd->first_minor = 0;
	//blocDevice.gd->fops = &bd_ops;
	blockDevice.gd->private_data = &blockDevice;
	strcpy (blockDevice.gd->disk_name, "bd0");
	set_capacity(blockDevice.gd, nsectors*(hardsect_size/KERNEL_SECTOR_SIZE));
	blockDevice.gd->queue = Queue;
	add_disk(blockDevice.gd);

    return 0;
}




module_init(block_driver_entry);
module_exit(block_driver_exit);


