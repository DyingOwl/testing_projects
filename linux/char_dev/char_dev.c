/* 
 * file name: char_dev.c
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/moduleparam.h>

#include <linux/fs.h>      //files
#include <linux/device.h>  //device_create
#include <linux/uaccess.h> //copy_to(from)_user
#include <linux/poll.h>    //poll

#define DEVICE_NAME "char"
#define BUFF_MAX 1024

static int char_dev_open(struct inode *inode, struct file *filp);
static int char_dev_release(struct inode *inode, struct file *filp);

static ssize_t char_dev_read(struct file *filp, \
	char __user *buf, size_t len, loff_t *offset);
static ssize_t char_dev_write(struct file *filp, \
	const char __user *buf, size_t len, loff_t *offset);

static __poll_t char_dev_poll(struct file *filp, struct poll_table_struct *wait);

MODULE_AUTHOR("maxinyu");
MODULE_LICENSE("GPL");

static volatile int flag = 0;
static int major = 0;
static int minor = 0;
static struct  class *cls;
static struct device *dev;
static char data_buff[BUFF_MAX] = {};

//defined in linux/fs.h: 1822
static const struct file_operations char_dev_ops ={
	.owner          = THIS_MODULE,
	.open           = char_dev_open,
	.read           = char_dev_read,
	.write          = char_dev_write,
	//.unlocked_ioctl = ,
	.release        = char_dev_release,
	.poll           = char_dev_poll,
};

static int __init char_dev_init(void)
{
	int ret = 0;

	if((major = register_chrdev(0, DEVICE_NAME "_dev", &char_dev_ops)) < 0)
	{
		printk(KERN_ERR "register_chrdev error %d\n", major);
		ret = major;
		goto ERR_EXIT_LEVEL_0;
	}

	cls = class_create(THIS_MODULE, DEVICE_NAME "_class");
	if(IS_ERR(cls))
	{
		printk(KERN_ERR "class_create error\n");
		ret = -EPERM;
		goto ERR_EXIT_LEVEL_1;
	}
	
/* 
 * func values in linux/device.h: 1576
struct device *device_create(struct class *cls, struct device *parent,
			     dev_t devt, void *drvdata,
			     const char *fmt, ...);
 */
	dev = device_create(cls, NULL, MKDEV(major, minor), NULL, DEVICE_NAME "_dev%d", minor);
	if(IS_ERR(dev))
	{
		printk(KERN_ERR "device_create error\n");
		ret = -EPERM;
		goto ERR_EXIT_LEVEL_2;
	}

	printk(KERN_NOTICE "char_dev_init finish\n");
	goto ERR_EXIT_LEVEL_0;

//ERR_EXIT_LEVEL_ALL:
	device_destroy(cls, MKDEV(major, minor));
ERR_EXIT_LEVEL_2:
	class_destroy(cls);
ERR_EXIT_LEVEL_1:
	unregister_chrdev(major, DEVICE_NAME "_dev");
ERR_EXIT_LEVEL_0:
	return ret;
}

static void __exit char_dev_exit(void)
{

	device_destroy(cls, MKDEV(major, minor));
	class_destroy(cls);
	unregister_chrdev(major, DEVICE_NAME "_dev");

	printk(KERN_NOTICE "char_dev_exit finish\n");
	return;
}

int char_dev_open(struct inode *inode, struct file *filp)
{
	printk(KERN_NOTICE "char_dev_open finish\n");

	return 0;
}

int char_dev_release(struct inode *inode, struct file *filp)
{
	printk(KERN_NOTICE "char_dev_release finish\n");

	return 0;
}

static DECLARE_WAIT_QUEUE_HEAD(char_dev_waitq);

static ssize_t char_dev_read(struct file *filp, \
	char __user *buf, size_t len, loff_t *offset)
{
	int ret = 0;

	wait_event_interruptible(char_dev_waitq, flag);

	if(len > BUFF_MAX)
	{
		len = BUFF_MAX;
	}

	if((ret = copy_to_user(buf, data_buff, len)) > 0)
	{
		printk(KERN_ERR "copy_to_user error, left %d words\n", ret);
		ret = -EACCES;
	}
	else
	{
		ret = len;
		flag = 0;
	}

	printk(KERN_NOTICE "char_dev_read finish\n");
	return ret;
}

static ssize_t char_dev_write(struct file *filp, \
	const char __user *buf, size_t len, loff_t *offset)
{
	int ret = 0;

	if(len > BUFF_MAX)
	{
		len = BUFF_MAX;
	}

	if((ret = copy_from_user(data_buff, buf, len)) > 0)
	{
		printk(KERN_ERR "copy_from_user error, left %d words\n", ret);
		ret = -EACCES;
	}
	else
	{
		ret = len;
		flag = 1;
		wake_up_interruptible(&char_dev_waitq);
	}

	printk(KERN_NOTICE "char_dev_write finish\n");
	return ret;
}

static __poll_t char_dev_poll(struct file *filp, struct poll_table_struct *wait)
{
	unsigned int mask = 0;
	poll_wait(filp, &char_dev_waitq, wait);

	if(flag)
	{
		mask |= POLLIN | POLLRDNORM;
	}

	return mask;
}


module_init(char_dev_init);
module_exit(char_dev_exit);

MODULE_DESCRIPTION("test char dev mod");
MODULE_ALIAS("UnicChar");
