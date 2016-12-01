/* linux/drivers/char/tiny210_keys.c


   Copyright (c) 2001,2002 Christer Weinigel <wingel@nano-system.com>,
*/

#include <linux/fs.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/cdev.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/mutex.h>
#include <linux/nsc_gpio.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/poll.h>
#include <linux/semaphore.h>
#include <linux/mutex.h>
#include <asm/uaccess.h>
#include <asm/signal.h>

#include <mach/map.h>
#include <mach/gpio.h>
#include <mach/regs-clock.h>
#include <mach/regs-gpio.h>

#define DEVNAME "tiny210-keys"

MODULE_AUTHOR("Nick <hbjsxieqi@163.com>");
MODULE_DESCRIPTION("TINY210 KEYs Driver");
MODULE_LICENSE("GPL");


struct key_desc{
	int pin;
	int number;
	char *name;
};

static struct key_desc keys[8] = {
	{S5PV210_GPH2(0), 0, "KEY1"},
	{S5PV210_GPH2(1), 1, "KEY2"},
	{S5PV210_GPH2(2), 2, "KEY3"},
	{S5PV210_GPH2(3), 3, "KEY4"},
	{S5PV210_GPH3(0), 4, "KEY5"},
	{S5PV210_GPH3(1), 5, "KEY6"},
	{S5PV210_GPH3(2), 6, "KEY7"},
	{S5PV210_GPH3(3), 7, "KEY8"},
};

static int major;		/* default to dynamic major */
module_param(major, int, 0);
MODULE_PARM_DESC(major, "Major device number");


static struct class *tiny210_keys_class;
static struct device *tiny210_keys_class_dev; 

//GPH2_0/1/2/3为KEY1-4引脚
//GPH3_0/1/2/3为KEY6-8引脚

#define GPH2CON		0xE0200C40
#define GPH2DAT		0xE0200C44
#define GPH3CON		0xE0200C60
#define GPH3DAT		0xE0200C64

//对应虚拟地址
//static volatile unsigned long *gph2con_va, *gph2dat_va, *gph3con_va, *gph3dat_va;

static struct cdev *tiny210_keys_cdev;

static DECLARE_WAIT_QUEUE_HEAD(key_waitq);
static volatile int ev_press = 0;

static struct fasync_struct *fasync_q;

//使用原子变量操作实现互斥
//static atomic_t canopen = ATOMIC_INIT(1);

//定义信号量
DEFINE_SEMAPHORE(key_semaphore);
//定义互斥信号量
DEFINE_MUTEX(key_mutex);

static struct timer_list key_timer;

static volatile char key_values[8] = {0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88};
static struct key_desc *key = NULL;
//KEY1-8按下输出0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x08
//KEY1-8释放输出0x81 0x82 0x83 0x84 0x85 0x86 0x87 0x88
static irqreturn_t tiny210_keys_interrupt(int irq, void *dev_id)
{
	key = (struct key_desc *)dev_id;

	mod_timer(&key_timer, jiffies + HZ / 100);//定时时间10ms

	return IRQ_HANDLED;
}

static void key_timer_handler(unsigned long data)
{
	if (!key)
		return;

	key_values[key->number] = gpio_get_value(key->pin) == 0 ? (key->number + 1) : (key->number + 1 + 0x80);
	   
	ev_press = 1;
	wake_up_interruptible(&key_waitq);
		 
	//异步通知
	kill_fasync(&fasync_q, SIGIO, POLL_IN);
	
//	printk("KEY%d: %s(0x%02x)\n", key->number + 1, key_values[key->number] & 0x80 ? "UP  " : "DOWN", key_values[key->number]);
}

static int tiny210_keys_open(struct inode *inode, struct file *file)
{
	int irq = 0;
	int i;
	int err;

//	if (!atomic_dec_and_test(&canopen)){
//		atomic_inc(&canopen);
//
//		return -EBUSY;
//	}

	
	if (file->f_flags & O_NONBLOCK){
		//如果以非阻塞方式打开则获取信号失败就立即返回，不休眠
		//返回0表示获取信号量成功。
		if (!mutex_trylock(&key_mutex)) 
			return -EBUSY;
		
		//这里要注意返回值，down_trylock返回0表示获取信号量成功，而mutex_trylock返回1表示获取信号量成功。
//		if (down_trylock(&key_semaphore))
//				return -EBUSY;
	} else {
		mutex_lock(&key_mutex);
//		down(&key_semaphore);
	}

	for (i=0; i<ARRAY_SIZE(keys); i++) {
		if (!keys[i].pin)
			continue;

		irq = gpio_to_irq(keys[i].pin);
		err = request_irq(irq, tiny210_keys_interrupt, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, keys[i].name, (void *)&keys[i]);
		if (err) {
			printk("%s(%d) failed to request IRQ: %d.\n", __FILE__, __LINE__, irq);

			break;
		}			
	}

	//释放已经申请的IRQ
	if (err) {
		i--;
		for (; i>=0; i--){
			if (!keys[i].pin)
				continue;

			irq = gpio_to_irq(keys[i].pin);
			disable_irq(irq);
			free_irq(irq, (void *)&keys[i]);
		}

		return -EBUSY;
	}

	printk("tiny210-keys: open!\n");
	
	return nonseekable_open(inode, file);
}

static int tiny210_keys_close(struct inode *inode, struct file *file)
{
	int irq, i;

	for (i=0; i<ARRAY_SIZE(keys); i++){
		if (!keys[i].pin)
			continue;

		irq = gpio_to_irq(keys[i].pin);
		free_irq(irq, (void *)&keys[i]);
	}
	
	//atomic_inc(&canopen);
	//up(&key_semaphore);
	mutex_unlock(&key_mutex);

	return 0;
}

static ssize_t tiny210_keys_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
	int err;

	if (8 != size)
		return -EINVAL;

	wait_event_interruptible(key_waitq, ev_press);

	ev_press = 0;

	err = copy_to_user(buf, (const void *)key_values, 8);

	return err ? -EFAULT : size;
}


static unsigned int tiny210_keys_poll(struct file *fp, poll_table * wait)
{
	unsigned int mask = 0;

	//仅仅是把应用进程挂在key_waitq等待队列，不会引起休眠
	poll_wait(fp, &key_waitq, wait);

	if (ev_press){
		mask |= POLLIN | POLLRDNORM;
	}

	return mask;
}

static int tiny210_keys_fasync(int fd, struct file *file, int on)
{
	return fasync_helper(fd, file, on, &fasync_q);
}

static const struct file_operations tiny210_keys_fops = {
	.owner		= THIS_MODULE,
	.open		= tiny210_keys_open,
	.read		= tiny210_keys_read,
	.release	= tiny210_keys_close,
	.poll		= tiny210_keys_poll,
	.fasync		= tiny210_keys_fasync,
};


static int __init tiny210_keys_init(void)
{
	int rc;
	dev_t devid;

	printk("tiny210-keys: init!\n");

	if (major) {
		devid = MKDEV(major, 0);
		rc = register_chrdev_region(devid, 1, DEVNAME);
	} else {
		rc = alloc_chrdev_region(&devid, 0, 1, DEVNAME);
		major = MAJOR(devid);
	}

	if (rc < 0) {
		printk("register-chrdev failed: %d\n", rc);
		return -EBUSY;
	}
	if (!major) {
		major = rc;
		printk("got dynamic major %d\n", major);
	}


	/* ignore minor errs, and succeed */
	tiny210_keys_cdev = cdev_alloc();
	if (!tiny210_keys_cdev){
		printk("failed to alloc cdev!\n");
		
		rc =  -ENOMEM;
		goto undo_chrdev_region;
	}
	cdev_init(tiny210_keys_cdev, &tiny210_keys_fops);
	tiny210_keys_cdev->owner = THIS_MODULE;
	cdev_add(tiny210_keys_cdev, devid, 1);

	tiny210_keys_class = class_create(THIS_MODULE, "tiny210_keys");
	tiny210_keys_class_dev = device_create(tiny210_keys_class, NULL, devid, NULL, "nick_keys");

	init_timer(&key_timer);
	key_timer.expires = get_jiffies_64();
	key_timer.function = key_timer_handler;
	key_timer.data = 0;

	add_timer(&key_timer);

	//转化为虚拟地址
//	gph2con_va = (volatile unsigned long *)ioremap(GPH2CON, 4);
//	gph2dat_va = (volatile unsigned long *)ioremap(GPH2DAT, 4);

//	gph3con_va = (volatile unsigned long *)ioremap(GPH3CON, 4);
//	gph3dat_va = (volatile unsigned long *)ioremap(GPH3DAT, 4);


	return 0;

undo_chrdev_region:
	unregister_chrdev_region(MKDEV(major,0), 1);

	return rc;
}

static void __exit tiny210_keys_cleanup(void)
{
	printk("tiny210-keys: cleanup!\n");

	cdev_del(tiny210_keys_cdev);
	unregister_chrdev_region(MKDEV(major,0), 1);
	
	device_unregister(tiny210_keys_class_dev);
	class_destroy(tiny210_keys_class);

	del_timer(&key_timer);

//	iounmap(gph2con_va);
//	iounmap(gph2dat_va);
}

module_init(tiny210_keys_init);
module_exit(tiny210_keys_cleanup);
