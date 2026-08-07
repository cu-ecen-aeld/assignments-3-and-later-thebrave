/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include <linux/init.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/semaphore.h>
#include <linux/types.h>

#include "aesdchar.h"

int aesd_major = 0; // use dynamic major
int aesd_minor = 0;

MODULE_AUTHOR("Jean Berniolles"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
	PDEBUG(">> aesd_open");

	struct aesd_dev *dev;
	dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
	filp->private_data = dev;

	return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
	PDEBUG(">> aesd_release");

	// Nothing to do here, but we must have this function or the module will not
	// load.

	return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
		  loff_t *f_pos)
{
	ssize_t retval = 0;
	PDEBUG(">> aesd_read %zu bytes with offset %lld", count, *f_pos);

	// Retrieve the device structure from the file pointer
	struct aesd_dev *dev = filp->private_data;

	size_t entry_offset_byte_rtn;
	struct aesd_buffer_entry *entry =
		aesd_circular_buffer_find_entry_offset_for_fpos(
			&dev->buffer, *f_pos, &entry_offset_byte_rtn);

	if (entry == NULL) {
		PDEBUG("No entry found for offset %lld", *f_pos);
		PDEBUG("<< aesd_read ret=0");
		return 0; // No data to read
	} else {
		PDEBUG("Entry found for offset %lld: buffptr=%p, size=%zu, entry_offset_byte_rtn=%zu",
		       *f_pos, entry->buffptr, entry->size,
		       entry_offset_byte_rtn);
	}

	if (entry->buffptr != NULL) {
		int fail_count =
			copy_to_user(buf + retval, entry->buffptr, entry->size);
		WARN_ON(fail_count != 0);
		retval += entry->size;
	} else {
		PDEBUG("bug?: no data in entry");
		PDEBUG("<< aesd_read ret=-EFAULT");
		return -EFAULT;
	}

	*f_pos += retval;
	PDEBUG("<< aesd_read ret=%zu, new fpos=%lld", retval, *f_pos);
	return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
		   loff_t *f_pos)
{
	ssize_t retval = -ENOMEM;
	PDEBUG(">> aesd_write %zu bytes with offset %lld", count, *f_pos);

	// Retrieve the device structure from the file pointer
	struct aesd_dev *dev = filp->private_data;
	BUG_ON(!dev);

	// If there is an incomplete write in progress (ie: write_buf != NULL),
	// then we need to append the new data to the existing write buffer
	if (dev->write_buf != NULL) {
		// Reallocate the write buffer to hold the new data
		PDEBUG("reallocate the write buffer to hold the new data");
		char *new_buf = krealloc(dev->write_buf,
					 dev->write_buf_size + count,
					 GFP_KERNEL);
		if (new_buf == NULL) {
			PDEBUG("failed to reallocate write buffer for %zu bytes",
			       dev->write_buf_size + count);
			retval = -ENOMEM;
			goto out;
		}
		dev->write_buf = new_buf;
		int fail_count = copy_from_user(
			dev->write_buf + dev->write_buf_size, buf, count);
		WARN_ON(fail_count != 0);
		dev->write_buf_size += count;
	} else {
		// Allocate a new write buffer and copy the data into it
		PDEBUG("allocate a new write buffer and copy the data into it");
		dev->write_buf = kzalloc(count, GFP_KERNEL);
		if (dev->write_buf == NULL) {
			PDEBUG("failed to allocate write buffer for %zu bytes",
			       count);
			retval = -ENOMEM;
			goto out;
		}

		int fail_count = copy_from_user(dev->write_buf, buf, count);
		WARN_ON(fail_count != 0);
		dev->write_buf_size = count;
	}

	// Is the write complete ? Check if the last character is a newline
	if (dev->write_buf[dev->write_buf_size - 1] == '\n') {
		// Write is complete, add the entry to the circular buffer
		PDEBUG("write is complete, add the entry to the circular buffer");
		struct aesd_buffer_entry new_entry;
		new_entry.buffptr = dev->write_buf;
		new_entry.size = dev->write_buf_size;

		aesd_circular_buffer_add_entry(&dev->buffer, &new_entry);

		// Reset the write buffer
		dev->write_buf = NULL;
		dev->write_buf_size = 0;
	} else {
		// Write is not complete, keep the write buffer for the next write
		PDEBUG("write is not complete, keep the write buffer for the next write");
	}

	retval = count;

out:
	PDEBUG("<< aesd_write ret=%zu", retval);
	*f_pos += retval;
	return retval;
}
struct file_operations aesd_fops = {
	.owner = THIS_MODULE,
	.read = aesd_read,
	.write = aesd_write,
	.open = aesd_open,
	.release = aesd_release,
	.llseek = default_llseek,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
	int err, devno = MKDEV(aesd_major, aesd_minor);

	cdev_init(&dev->cdev, &aesd_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &aesd_fops;
	err = cdev_add(&dev->cdev, devno, 1);
	if (err) {
		printk(KERN_ERR "Error %d adding aesd cdev", err);
	}
	return err;
}

int aesd_init_module(void)
{
	dev_t dev = 0;
	int result;
	result = alloc_chrdev_region(&dev, aesd_minor, 1, "aesdchar");
	aesd_major = MAJOR(dev);
	if (result < 0) {
		printk(KERN_WARNING "Can't get major %d\n", aesd_major);
		return result;
	}
	memset(&aesd_device, 0, sizeof(struct aesd_dev));

	/**
   * TODO: initialize the AESD specific portion of the device
   */
	aesd_circular_buffer_init(&aesd_device.buffer);
	sema_init(&aesd_device.lock, 1);

	result = aesd_setup_cdev(&aesd_device);

	if (result) {
		unregister_chrdev_region(dev, 1);
	}
	return result;
}

void aesd_cleanup_module(void)
{
	dev_t devno = MKDEV(aesd_major, aesd_minor);

	cdev_del(&aesd_device.cdev);

	/**
   * TODO: cleanup AESD specific poritions here as necessary
   */

	unregister_chrdev_region(devno, 1);
}

module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
