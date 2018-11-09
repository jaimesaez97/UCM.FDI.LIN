#include<linux/syscalls.h>		/* For the macro SYSCALL_DEFINEi()*/
#include<linux/kernel.h>
#include <linux/module.h> 
#include <asm-generic/errno.h>
#include <linux/init.h>
#include <linux/tty.h>      	/* For fg_console */
#include <linux/kd.h>       	/* For KDSETLED */
#include <linux/vt_kern.h>

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Ledctl_syscall");
MODULE_AUTHOR("Jaime Sáez de Buruaga");

/**********************************************************************************/
/**********************************************************************************/
//							MODLEDS
#define ALL_LEDS_ON 0x7
#define ALL_LEDS_OFF 0

struct tty_driver* kdb_driver = NULL;	/* Driver para simular proc_create */

	/* Get driver handler */
struct tty_driver* get_kdb_driver_handler(void)
{
	printk(KERN_INFO "modleds: loading module\n");
	printk(KERN_INFO "modleds: fgconsole is %x\n", fgconsole);
		/* Returns the driver associated */
   return vc_cons[fg_console].d->port.tty->driver;
}
	/* Set led state to that specified by mask */
static inline int set_leds(struct tty_driver* handler, unsigned int mask)
{		/* Returns the operations of I/O controller asociated to the driver*/
	return (handler->ops->ioctl) (vc_cons[fg_console].d->port.tty, KDSETLED,mask);
}

static int __init modleds_init(void)
{		/* Asociates the driver to kdb_drriver */
	kdb_driver = get_kdb_driver_handler();
		/* Turn all leds on */
	set_leds(kdb_driver, ALL_LEDS_ON);
	return 0;
}

static void __exit modleds_exit(void)
{		/* Turn all leds off */
	set_leds(kdb_driver, ALL_LEDS_OFF);
}
	
module_init(modleds_init);	/* Macro that associates the loading to MODLEDS_INIT */
module_exit(modleds_exit);	/* Macro that associates the unloading to MODLEDS_EXIT */
/**********************************************************************************/
/**********************************************************************************/
//							MASK_FILTER
unsigned int convert(unsigned int mask){
	int i = 0;
	int converted_mask = 0;
	for(i = 0; i < 3; ++i){
			/* if (bit-i € MASK == 1)*/
		if(num & (0x1 << i)) {
			switch(i) {
				case 0: converted_mask |= 1; break;	/* if 0-bit == 1 -> SCROL LOCK = 1*/
				case 1: converted_mask |= 4; break;	/* if 1-bit == 1 -> NUMS_LOCK  = 1*/
				case 2: converted_mask |= 2; break;	/* if 2-bit == 1 -> CAPS_LOCK  = 1*/
				default: return -EINVAL;
			}
		}
	}
	return converted_mask;
}




/**********************************************************************************/
/**********************************************************************************/
//							SYSCALL_DEFINE


SYSCALL_DEFINE1(ledctl, unsigned int, mask)
{
	printk(KERN_DEBUG "Ledctl_Syscall");
		/* Error comprobation */
	if((mask < 0) || (mask > 7))
		return -EINVAL;
		/* First we converty the mask to get the correct one */
	unsigned int cv_mask = 0;
	cv_mask = convert(mask);
		/* Calling to:
			Modleds.c // static inline int set_leds(struct tty_driver* handler, unsigned int mask)*/
	kdb_driver = get_kdb_driver_handler();
	if(set_leds(kdb_driver, cv_mask) < 0)
		return -ENOTSUPP;	/* Failed */

	return 0;
}
