
#include <linux/init.h>				/*  */
#include <linux/module.h>			/*  */
#include <linux/kernel.h>			/*  */

#include <linux/device.h>			/* Device types and classes */
#include <linux/cdev.h>				/* Device descriptors */
#include <linux/fs.h>					/* Allocating and freeing device numbers */
#include <linux/errno.h>			/* Error codes */
#include <asm/uaccess.h>			/* User access - file operations */

#include <linux/slab.h>				/* Kernel memory allocation */
#include <linux/ioport.h>			/* I/O memory allocation and mapping */
#include <asm/io.h>						/* Accessing I/O memory */

#include <linux/semaphore.h>	/* Mutual exclusion mechanism */
#include <linux/wait.h>				/* Simple sleeping mechanism */
#include <linux/sched.h>			/*  */
#include <linux/interrupt.h>	/*  */
#include <linux/signal.h>			/*  */

//#include "sys_timer.h"


/* Hardware specifics */
/*****************************************************************************/

/* Reference: BCM2835 ARM Peripherals, page 172 */
#define 	SYSTEM_TIMER_BASE 	(0x20003000)   /* system timer address */

/* The following are OFFSETS to be used with the SYSTEM_TIMER_BASE address */
#define		SYSTEM_TIMER_CS   	(0x0)   /* 	timer control/status 			*/
#define		SYSTEM_TIMER_CLO  	(0x4)   /* 	timer lower 32 bits 			*/
#define 	SYSTEM_TIMER_CHI  	(0X8)   /* 	timer higher 32 bits 			*/
#define 	SYSTEM_TIMER_C0   	(0xC)   /* 	timer compare register 0 	*/
#define 	SYSTEM_TIMER_C1   	(0x10)  /* 	timer compare register 1 	*/
#define 	SYSTEM_TIMER_C2   	(0x14)  /* 	timer compare register 2 	*/
#define 	SYSTEM_TIMER_C3   	(0x18)  /* 	timer compare register 3 	*/

/* Reference: BCM2835 ARM Peripherals, page 112 */
#define  	INTERRUPT_REGS_BASE (0x2000B200)

/* The following are OFFSETS to be used with the INTERRUPT_REGS_BASE address */
#define 	 ENABLE_IRQs_1			(0x10)
#define 	DISABLE_IRQs_1			(0x1C)
// NOTE: the above are not all the registers, just the ones used here

/* The I/O memory (in bytes) we are reserving for hardware operations */
#define		SYSTEM_TIMER_SIZE		(28)
#define 	INTERRUPT_REGS_SIZE	(36)


/* Module specifics */
/*****************************************************************************/

#define DRIVER_NAME		 		"sys_timer"
#define DRIVER_DESC    		"A simple driver to expose the RPi System Timer to /dev"
#define DRIVER_LICENSE 		"Dual BSD/GPL"

#define DRIVER_AUTHOR  		"Radu Traian Jipa, <radu.t.jipa@gmail.com>"



/* Globals */
/*****************************************************************************/

/* Char device registration structure */
struct sys_timer_dev {
	unsigned char *data ;								/* contents of a char device */
	unsigned char minor ;								/* minor number identifier */
  unsigned char status ;							/* opened (busy) / unopened (free) */
	unsigned short int buffer_size ;		/* size of the device contents */
	unsigned short int block_size ;			/*  */
	struct semaphore sem ;							/* mutual exclusion semaphore */
	struct cdev cdev ;									/* kernel's internal structure of char devices */
};

/* Locals - driver specifics */
static const char *dev_names[] = { "CS", "CLO", "CHI", "C0", "C1", "C2", "C3" } ;
static const int number_of_devices = sizeof(dev_names) / sizeof (char*) ;
static const unsigned int dev_offsets[] = { SYSTEM_TIMER_CS,  SYSTEM_TIMER_CLO, 
																						SYSTEM_TIMER_CHI, SYSTEM_TIMER_C0, 
																						SYSTEM_TIMER_C1,  SYSTEM_TIMER_C2, 
																						SYSTEM_TIMER_C3 } ;

static unsigned int major ;								/* major number identifier of the driver */
static struct class *class ;							/*  */
static struct sys_timer_dev *stdevs ; 		/* an array containing our system timer's devices */

/* Virtual address mapping of the physical address */
static volatile uint32_t sys_timer_base ;	
static volatile uint32_t interrupt_controller ; 

/*  */
static wait_queue_head_t wait_queue ;

static u32 interval  = 100 ;
static unsigned int irqs_left = 10*10000, irq_count = 0, tasklet_count = 0 ;
static u32 stc1, stc1_drift, stc1_base ;

static pid_t  sent_to_pid ;
static struct task_struct *process ;


/*
 * Device file operations
 ******************************************************************************
 */

/* Method declarations for the file operations on our char devices */
int sys_timer_open 			(struct inode *inode, struct file *filp) ;
int sys_timer_close 		(struct inode *inode, struct file *filp) ;
ssize_t sys_timer_read 	(struct file *filp, char __user *buff, size_t count, loff_t *offp) ;
ssize_t sys_timer_write (struct file *filp, const char __user *buff, size_t count, loff_t *offp) ;

/* File operations structure to be linked with each device */
struct file_operations fops = {
	.owner 		= THIS_MODULE,
	.open 		= sys_timer_open,
	.release	= sys_timer_close,
	.read 		= sys_timer_read,
	.write 		= sys_timer_write,
	.poll 		= NULL, //sys_timer_poll,
};


/*
 *
 ******************************************************************************
 */
int sys_timer_open (struct inode *inode, struct file *filp)
{
	unsigned int i_major = imajor (inode) ;
	unsigned int i_minor = iminor (inode) ;
	
	printk (KERN_INFO "%s: Dev open()\n", dev_names[i_minor]) ;
	
	/* In case the inode parameter is "corrupted" */
	if (i_major != major || i_minor < 0 || i_minor >= number_of_devices)
	{
		printk (KERN_ALERT "ERROR %d: No such device!\n", -ENODEV) ;
		return -ENODEV ;
	}

	/* Check if this devices has been opened before */
	if (stdevs[i_minor].status != 0)
	{
		printk (KERN_ALERT "ERROR: This device is busy. Did someone forgot to close it?\n") ;
		return -ENODEV ;
	}
	else
		stdevs[i_minor].status++ ;

	/*  */
	filp -> private_data = &stdevs[i_minor] ;
	
	/* If the device is empty, allocate memory for future contents */
	if (stdevs[i_minor].data == NULL)
	{
		stdevs[i_minor].data = (unsigned char*) kzalloc (stdevs[i_minor].buffer_size * sizeof(unsigned char*), GFP_KERNEL);
		if (stdevs[i_minor].data == NULL)
		{
			printk (KERN_ALERT "ERROR: Could not allocate memory for device %s.\n", dev_names[i_minor]) ;
			return -ENOMEM ;
		} // if
	} // if

	/* Device opened successfully */
	return 0 ;
}


/*
 *
 ******************************************************************************
 */
int sys_timer_close (struct inode *inode, struct file *filp)
{
	unsigned int i_minor = iminor (inode) ;

	printk (KERN_INFO "%s: Dev close()\n", dev_names[i_minor]) ;

	stdevs[i_minor].status--;
	
	return 0 ;
}


/*
 *
 ******************************************************************************
 */
ssize_t sys_timer_read (struct file *filp, char __user *buff, size_t count, loff_t *unused)
{
	/*  */
	unsigned int hardware_value = 0 ;
	struct sys_timer_dev *stdev = (struct sys_timer_dev *) filp -> private_data ;
	
	/*  */
	//wait_event_interruptible (wait_queue, ioread32 (sys_timer_base) == 2) ;
	
	/* Read 4 bytes from the system timer's register (device) */
	hardware_value = ioread32 ((uint32_t *)(sys_timer_base + dev_offsets[stdev -> minor])) ;
	
	/* Copy and convert the integer value to the char device contents - itoa() */
	snprintf (stdev -> data, stdev -> buffer_size, "%u", hardware_value) ;
	
	printk (KERN_INFO "%s: Dev read()\n", dev_names[stdev -> minor]) ;
		
	/* Truncate the number of chracters to be read from device */
	if (count > stdev -> buffer_size)
		count = stdev -> buffer_size ;

	/* Kernel's safe way of transfering data from kernel space to user space */
	if (copy_to_user (buff, &(stdev -> data[0]), count) != 0)
		return -EFAULT ;

	if (stdev -> minor == 0)
	{
		printk (KERN_DEBUG "tasklet count is: %u\n", tasklet_count) ;
		printk (KERN_DEBUG "irqs left is: %u\n", irqs_left) ;
		printk (KERN_DEBUG "irq count is: %u\n", irq_count) ;
		//printk (KERN_ALERT "irq drift: %u\n", stc1_drift) ;
	}

	/* Return the number of bytes (characters) were read */
	return count ;
}


/*
 *
 ******************************************************************************
 */
ssize_t sys_timer_write (struct file *filp, const char __user *buff, size_t count, loff_t *unused)
{
	/*  */
	unsigned int value_to_write = 0 ;
	struct sys_timer_dev *stdev = (struct sys_timer_dev *) filp -> private_data ;
	
	/* Writes to the devices are atomic operations! */
	if (down_interruptible (&stdev -> sem) != 0)
		return -ERESTARTSYS ;
	
	/* Not safe to write to CS, CLO or CHI! */
	if (stdev -> minor >= 0 && stdev -> minor <= 2)
	{
		printk (KERN_ALERT "It is not yet advisable you write to this device.\n") ;
		return 0 ;
	}
	
	printk (KERN_INFO "%s: Dev write()\n", dev_names[stdev -> minor]) ;
		
	/*  */
	if (count > stdev -> buffer_size)
		count = stdev -> buffer_size ;

	/* Kernel's safe way of transfering data from user space to kernel space */
	if (copy_from_user (&(stdev -> data[0]), buff, count) != 0)
		return -EFAULT ;
		
	/* Copy and convert the device contents to an unsigned interger - atoi() */
	sscanf (&(stdev -> data[0]), "%u", &value_to_write) ;
	
	/* Write the 4 bytes of data to the system timer's register */
	iowrite32 (value_to_write, (uint32_t *)(sys_timer_base + dev_offsets[stdev -> minor])) ;
	

	sent_to_pid = current -> pid ;
		
	printk (KERN_INFO "This process called write(): %d\n", (int)sent_to_pid) ;

	if ((process = pid_task (find_vpid (sent_to_pid), PIDTYPE_PID)) == NULL)
		printk (KERN_ALERT "ERROR: Could not get process struct.\n") ;
	

	//if (stdev -> minor == 4)
		//stc1 = value_to_write ;
	
	/* Release the lock and return the number of bytes (characters) writen */
	up (&stdev -> sem) ;
	return count ;
}


/*
 *
 ******************************************************************************
 */
void set_next_timer_event (unsigned long unused)
{
	//printk (KERN_ALERT "IRQ tasklet: irqs_left = %d | irq_count = %d\n", irqs_left, irq_count) ;
	tasklet_count++ ;

	if (irqs_left-- > 1)
	{
		stc1 = ioread32 ((uint32_t *)(sys_timer_base + dev_offsets[4])) ;
		iowrite32 ((stc1 + interval), (uint32_t *)(sys_timer_base + dev_offsets[4])) ;		
	}
	else
		printk (KERN_INFO "Finished IRQs. Gimmie more! \n") ;
}

//DECLARE_TASKLET_DISABLED (set_next_event, set_next_timer_event, 0) ;
//struct tasklet_struct my_tasklet ;


/*
 *
 ******************************************************************************
 */
static irqreturn_t interrupt_handler (int irq, void *dev_id, struct pt_regs *regs)
{
	//wake_up_interruptible (&wait_queue) ;
  irq_count++ ;
	
	if (irqs_left-- > 1)
	{
		unsigned int stc12 = ioread32 ((uint32_t *)(sys_timer_base + dev_offsets[4])) ;
		iowrite32 ((stc12 + interval), (uint32_t *)(sys_timer_base + dev_offsets[4])) ;		
		
		/* Signal marking the arrival of an interrupt */
		if (send_sig_info (SIGUSR1, SEND_SIG_FORCED, process) < 0)
			printk (KERN_ALERT "ERROR: Could not send the signal.\n") ;		
	}
	else
	{
		printk (KERN_INFO "Finished IRQs. Gimmie more! \n") ;

		/* Signal marking end of interrupts */
		if (send_sig_info (SIGUSR2, SEND_SIG_FORCED, process) < 0)
			printk (KERN_ALERT "ERROR: Could not send the signal.\n") ;
	}

	//tasklet_hi_schedule (&set_next_event) ;
	/* Acknowledge the interrupt on C1 */
	iowrite16 (1 << 1, (uint32_t *)sys_timer_base) ;
	
	/* Return IRQ serviced code */
	return IRQ_HANDLED ;
}


/*
 * 
 *********************************************************************************
 */
void set_real_time_priority (unsigned int priority)
{
  struct sched_param scheduler ;

  memset (&scheduler, 0, sizeof(scheduler)) ;

  scheduler.sched_priority = priority ;

  sched_setscheduler (current, SCHED_RR, &scheduler) ;
}



/*
 *
 ******************************************************************************
 */
static int create_devices (void)
{
	int minor, err = -1 ;

  /* Create and initialise all the char devices of the system timer */
	for (minor = 0 ; minor < number_of_devices ; minor++)
	{
		/* Device type and Major + Minor indentifier of the current device */
		dev_t devno = MKDEV (major, minor) ;
		
		/* Initialise the device's fields */
		stdevs[minor].data 				= NULL ;
		stdevs[minor].status 			= 0 ;
		stdevs[minor].minor 			= minor ;
		stdevs[minor].buffer_size = 11 ;     /* 2^32 = 4294967296 = 10 char bytes + 1 EOF */
		stdevs[minor].block_size  = 11 ;

		/* First initialise the MUTEX before the device to avoid a race condition
		   where the smaphore could be accessed before it is ready. */
		sema_init (&stdevs[minor].sem, 1) ;
		
		/* Initialise the char device structure */
		cdev_init (&stdevs[minor].cdev, &fops) ;
		stdevs[minor].cdev.owner = THIS_MODULE ;
		
		/* Tell the kernel about this device and add it to the system */
		if ((err = cdev_add (&stdevs[minor].cdev, devno, 1)) < 0)
		{
			printk (KERN_ALERT "ERROR %d: Could not add device %s!\n", err, dev_names[minor]) ;
			goto fail ;
		} // if

		/* no parent device, no additional data */
		if (device_create (class, NULL, devno, NULL, dev_names[minor]) == NULL)
		{
			printk (KERN_ALERT "ERROR: Could not create device %s!\n", dev_names[minor]) ;
			goto fail ;
		} // if
	} // for

	/* Devices created successfully */
	return 0 ;

fail:
	return err ;
}


/*
 *
 ******************************************************************************
 */
static void destroy_devices (void)
{
	int minor ;

	/*  */
	for (minor = 0 ; minor < number_of_devices ; minor++)
	{
		/*  */
		device_destroy (class, MKDEV (major, minor)) ;
		/* Remove the device from the system */
		cdev_del (&stdevs[minor].cdev) ;
    /* Free the kernel memory of the device */
		kfree (stdevs[minor].data) ;
	} // for

	/* Finaly, free the memory of the structure */
	kfree (stdevs) ;
}


/*
 *
 ******************************************************************************
 */
static int __init initialisation (void)
{
	int err = -1 ;
	dev_t dev ;

	printk (KERN_ALERT "Hello world!\n") ;

	/*  */
	if ((err = alloc_chrdev_region (&dev, 0, number_of_devices, DRIVER_NAME)) < 0)
	{
		printk (KERN_ALERT "ERROR %d: Failed to allocate device region!\n", err) ;
		goto fail ;
	}
	
	/*  */
	if ((class = class_create (THIS_MODULE, DRIVER_NAME)) == NULL)
	{
		printk (KERN_ALERT "ERROR: Could not create device class!\n") ;
		goto fail ;
	}
	
	major = MAJOR (dev) ;
	
	/* Allocate an array of devices for the system timer */
	stdevs = (struct sys_timer_dev *) kzalloc 
						(number_of_devices * sizeof(struct sys_timer_dev), GFP_KERNEL) ;
	if (stdevs == NULL)
	{
		printk (KERN_ALERT "Could not allocate memory for system timer devices!\n") ;
		err = -ENOMEM ;
		goto fail ;
	}

	/*  */
	if ((err = create_devices ()) < 0)
		goto fail ;


	/*  */
	if ((err = check_mem_region (SYSTEM_TIMER_BASE, SYSTEM_TIMER_SIZE)) < 0)
	{
		printk (KERN_ALERT "ERROR %d: Mem region is busy and might not be able to allocate!\n", err) ;
		return err ;
	}

	/*  */
	if ((err = check_mem_region (INTERRUPT_REGS_BASE, INTERRUPT_REGS_SIZE)) < 0)
	{
		printk (KERN_ALERT "ERROR %d: Mem region is busy and might not be able to allocate!\n", err) ;
		return err ;
	}
	
	/*  */
	if (request_mem_region (SYSTEM_TIMER_BASE, SYSTEM_TIMER_SIZE, "sys_timer_driver") == NULL)
	{
		printk (KERN_ALERT "ERROR: Could not request I/O memory for System Timer.\n") ;
		return -ENOMEM ;
	}

	/*  */
	if (request_mem_region (INTERRUPT_REGS_BASE, INTERRUPT_REGS_SIZE, "interrupt_controller") == NULL)
	{
		printk (KERN_ALERT "ERROR: Could not request I/O memory for Interrupt Controller.\n") ;
		return -ENOMEM ;
	}

	/*  */
	if ((sys_timer_base = (uint32_t) ioremap (SYSTEM_TIMER_BASE, SYSTEM_TIMER_SIZE)) < 0)
	{
		printk (KERN_ALERT "ERROR: Could not obtain I/O memory reference for CS.\n") ;
		return -ENOMEM ;
	}

	/*  */
	if ((interrupt_controller = (uint32_t) ioremap (INTERRUPT_REGS_BASE, INTERRUPT_REGS_SIZE)) < 0)
	{
		printk (KERN_ALERT "ERROR: Could not obtain I/O memory reference for the Interrupt Controller.\n") ;
		return -ENOMEM ;
	}

	init_waitqueue_head (&wait_queue) ;
	
	/*  */
	if (request_irq (1, (irq_handler_t ) interrupt_handler, IRQF_DISABLED | IRQF_TIMER, "System Timer IRQ Handler", &stdevs[0]) != 0)
		printk (KERN_ALERT "ERROR: cannot assign IRQ number!\n") ;
	
	//tasklet_enable (&set_next_event) ;

	set_real_time_priority (1) ;
	printk (KERN_INFO "current process priority is: %d\n", current -> prio) ;
	printk (KERN_INFO "current realtime prio is: %d\n", current -> rt_priority) ;
	
	/* Initialisation successful */
	return 0 ;

/*  */
fail:
	destroy_devices () ;
	class_destroy (class) ;
	unregister_chrdev_region (dev, 1) ;
	return err ;
}


/*
 *
 ******************************************************************************
 */
static void __exit cleanup (void)
{
	printk (KERN_ALERT "Goodbye, cruel world!\n") ;

	/*  */
	//tasklet_disable (&set_next_event) ;
	//tasklet_kill (&set_next_event) ;
	free_irq (1, &stdevs[0]) ;
	destroy_devices () ;
	class_destroy (class) ;
	unregister_chrdev_region (MKDEV (major, 0), 1) ;
	iounmap ((uint32_t *)sys_timer_base) ;
	iounmap ((uint32_t *)interrupt_controller) ;
	release_mem_region (SYSTEM_TIMER_BASE, SYSTEM_TIMER_SIZE) ;
	release_mem_region (INTERRUPT_REGS_BASE, INTERRUPT_REGS_SIZE) ;
}


/*
 *
 ******************************************************************************
 */
module_init (initialisation) ;
module_exit (cleanup) ;


/*  */
MODULE_LICENSE (DRIVER_LICENSE) ;

/*  */
MODULE_AUTHOR (DRIVER_AUTHOR) ;
MODULE_DESCRIPTION (DRIVER_DESC) ;
