#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/vmalloc.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/input.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include "cdata_ioctl.h"

#define	BUF_SIZE	(128)
#define	LCD_SIZE	(320*240*4)

struct cdata_t {
    unsigned long *fb;
    unsigned char *buf;
    unsigned int  index;
    unsigned int  offset;
};

static int cdata_open(struct inode *inode, struct file *filp)
{
	int i;
	int minor;
	struct cdata_t *cdata;

	printk(KERN_INFO "CDATA: in open\n");

	minor = MINOR(inode->i_rdev);
	printk(KERN_INFO "CDATA: minor = %d\n", minor);

	cdata= kmalloc(sizeof(struct cdata_t), GFP_KERNEL);
	cdata->buf = kmalloc(BUF_SIZE, GFP_KERNEL);
	cdata->fb = ioremap(0x33f00000, 320*240*4);
	cdata->index = 0;
	cdata->offset = 0;

	filp->private_data = (void *)cdata;

	return 0;
}

static ssize_t cdata_read(struct file *filp, char *buf, size_t size, loff_t *off)
{
}

void flush_lcd(void *priv)
{
	struct cdata_t *cdata = (struct cdata *)priv;
	unsigned char *fb;
	unsigned char *pixel;
	int index;
	int offset;
	int i;

	fb = (unsigned char *)cdata->fb;
	pixel = cdata->buf;
	index = cdata->index;
	offset = cdata->offset;

	for (i = 0; i < index; i++) {
	    writeb(pixel[i], fb+offset);
	    offset++;
	    if (offset >= LCD_SIZE)
		offset = 0;
	}
	
	cdata->index = 0;
	cdata->offset = offset;
}

static ssize_t cdata_write(struct file *filp, const char *buf, size_t size, 
			loff_t *off)
{
	struct cdata_t *cdata = (struct cdata *)filp->private_data;
	unsigned char *pixel;
	unsigned int index;
	unsigned int i;

	pixel = cdata->buf;
	index = cdata->index;

	for (i = 0; i < size; i++) {
	    if (index >= BUF_SIZE) {
		cdata->index = index;
	        flush_lcd((void *)cdata);
		index = cdata->index;
	    }
	    copy_from_user(&pixel[index], &buf[i], 1);
	    index++;
	}

    	cdata->index = index;

	return 0;
}

static int cdata_close(struct inode *inode, struct file *filp)
{
	struct cdata_t *cdata = (struct cdata *)filp->private_data;
	
	flush_lcd((void *)cdata);

	kfree(cdata->buf);
	kfree(cdata);

	return 0;
}

static int cdata_ioctl(struct inode *inode, struct file *filp, 
		unsigned int cmd, unsigned long arg)
{
	struct cdata_t *cdata = (struct cdata *)filp->private_data;
	int n;
	unsigned long *fb;
	int i;

	switch (cmd) {
	    case CDATA_CLEAR:
	        n = *((int *)arg); // FIXME: dirty
		printk(KERN_INFO "CDATA_CLEAR: %d pixel\n", n);

		// FIXME: Lock
		fb = cdata->fb;
		// FIXME: unlock
		for (i = 0; i < n; i++)
		    writel(0x00ff00ff, fb++);

	        break;
	}
}

static struct file_operations cdata_fops = {	
	owner:		THIS_MODULE,
	open:		cdata_open,
	release:	cdata_close,
	read:		cdata_read,
	write:		cdata_write,
	ioctl:		cdata_ioctl,

};

int cdata_init_module(void)
{
	unsigned long *fb;
	int i;

	fb = ioremap(0x33f00000, 320*240*4);
	for (i = 0; i < 320*240; i++)
		writel(0x00ff0000, fb++);
	
	if (register_chrdev(121, "cdata", &cdata_fops) < 0) {
	    printk(KERN_INFO "CDATA: can't register driver\n");
	    return -1;
	}
	printk(KERN_INFO "CDATA: cdata_init_module\n");
	return 0;
}

void cdata_cleanup_module(void)
{
	unregister_chrdev(121, "cdata");
}

module_init(cdata_init_module);
module_exit(cdata_cleanup_module);

MODULE_LICENSE("GPL");
