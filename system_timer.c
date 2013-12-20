
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/errno.h>

#define DEVICE_NAME		"sys_timer"
#define DRIVER_DESC   "A simple driver to expose the RPi System Timer to /dev"
#define DRIVER_AUTHOR "Radu Traian Jipa, <radu.t.jipa@gmail.com>"


/* Globals */
static int device_major_number;


/*
 *
 ******************************************************************************
 */
static int __init initialisation (void)
{
	printk(KERN_ALERT "Hello world!\n") ;
	
	int err ;

	/* Dynamically allocate a major number (first param is 0) */
	if ((err = register_chrdev (0, DEVICE_NAME, &fops)) != 0)
	{
		printk (KERN_ALERT "ERROR: Device registration failed!") ;
		return err ;
	}
	else 
		device_major_number = err ;
	
	/* initialisation successful */
	return 0 ;
}


/*
 *
 ******************************************************************************
 */
static void __exit cleanup (void)
{
	printk(KERN_ALERT "Goodbye, cruel world!\n") ;
}


/*
 *
 ******************************************************************************
 */
module_init (initialisation) ;
module_exit (cleanup) ;


/*  */
MODULE_LICENSE ("GPL") ;

/*  */
MODULE_AUTHOR (DRIVER_AUTHOR) ;
MODULE_DESCRIPTION (DRIVER_DESC) ;
