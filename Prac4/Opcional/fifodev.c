#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/module.h>
#include <asm-generic/errno.h>
#include <linux/init.h>
#include <linux/tty.h>      /* For fg_console */
#include <linux/kfifo.h>
#include <linux/stat.h>

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("PRAC 4 -PARTE OPC");
MODULE_AUTHOR("JAIME SAEZ DE BURUAGA");

	/* GLobal Constants */
#define MAX_KBUF				256			/* Max tam kernel buffer */
#define MAX_CBUFFER_LEN 		256			/* Max tam cyclic buffer */ 
#define SUCCESS 				0			/* Success return number */
#define DEVICE_NAME 			"fifodev"	/* Character device name */

	/* Global Variables */
struct kfifo cbuffer;			/* Cyclic buffer */
int prod_count = 0;			/* Number of productor process (write) */
int cons_count = 0;			/* Number of consumer process */
struct semaphore mtx;			/* Mutual exclusion */
struct semaphore sem_prod;		/* Costumers waiting queue -> number of items */
struct semaphore sem_cons;		/* Productors waiting queue -> number of holes */
int nr_prod_waiting = 0;		/* Number of prdocutors waiting */
int nr_cons_waiting = 0;		/* Number of consumers waiting */

	/* CharDev variables */
dev_t start;
struct cdev* chardev_fifo = NULL;

static int fifoproc_open(struct inode *inode, struct file *file)
{			
	/* Lock process till productor opens his extreme */
	if(down_interruptible(&mtx))
		return -EINTR;

	if(file->f_mode & FMODE_READ)
	{
		if (cons_count > 0){
			up(&mtx);
			return -1;
		}
		cons_count++;
			/* Por si hay algún productor bloqueado */
		if(nr_prod_waiting > 0){	
			up(&sem_prod);
			nr_prod_waiting--;
		}

		while(prod_count < 1){
			nr_cons_waiting++;
			up(&mtx);
				/* Me quedo esperando en sem_cons hasta que
				   haya productores */
			if(down_interruptible(&sem_cons)){
					/*BACKUP*/
				down(&mtx);
				nr_cons_waiting--;
				up(&mtx);
				return -EINTR;
			}
			if(down_interruptible(&mtx)){
				nr_cons_waiting--;
				return -EINTR;
			}
		}

	} else {
		if (prod_count > 0){
			up(&mtx);
			return -1;
		}
		prod_count++;
			/* Por si hay algún consumidor bloqueado */
		if(nr_cons_waiting > 0){
			up(&sem_cons);
			nr_cons_waiting--;
		}

		while(cons_count < 1){
			nr_prod_waiting++;
			up(&mtx);
				/* Me quedo esperando en sem_prod hasta
				   que haya consumidores */
			if(down_interruptible(&sem_prod)){
				down(&mtx);
				nr_prod_waiting--;
				up(&mtx);
				return -EINTR;
			}
			if(down_interruptible(&mtx)){
				nr_prod_waiting--;
				return -EINTR;
			}

		}
	}
	up(&mtx);
	return 0;
}

static int fifoproc_release(struct inode *inode, struct file *file)
{
	/* Lock process till productor opens his extreme */
	if(down_interruptible(&mtx))
		return -EINTR;

	if(file->f_mode & FMODE_READ)
	{
		cons_count--;
			/* Unlock possible productor */
		if(nr_prod_waiting > 0){
			nr_prod_waiting--;
			up(&sem_prod);
		}

	} else {

		prod_count--;
			/* Desbloqueo posible consumidor bloqueado por espacio */
		if(nr_cons_waiting > 0){
			nr_cons_waiting--;
			up(&sem_cons);
		}
	}

	if (cons_count+prod_count==0)
		kfifo_reset(&cbuffer);

	up(&mtx);
	return 0;	
}

static ssize_t fifoproc_write(struct file *file, const char __user *buf, size_t len, loff_t *off)
{
	char kbuf[MAX_KBUF];

	if( (len > MAX_CBUFFER_LEN) || (len > MAX_KBUF) )
		return -ENOSPC;	/* Not Enough Space */
	if(copy_from_user(kbuf, buf, len))
		return -EINVAL;	/* Not Valid Argument */

/*********** COND_WAIT(PROD, MTX) ***********/
		/* mutex_lock */
	if(down_interruptible(&mtx))
		return -EINTR;	/* Interrupted System Call */

		/* Waits untill kfifo is free (there might be cons) */
	while( (kfifo_avail(&cbuffer) < len) && (cons_count > 0) )
	{							
		nr_prod_waiting++;
			/* Mutex Unlocked */
		up(&mtx);
		if(down_interruptible(&sem_prod))	/* There is any hole  -> ERROR */
		{		
			down(&mtx);
				/* Backup (when -EINTR) */ 
			nr_prod_waiting--;
			up(&mtx);
			return -EINTR;
		}
		if(down_interruptible(&mtx)){
			nr_prod_waiting--;
			return -EINTR;
		}
	}

		/* End of comunication error detectio (there might be consumers) */
	if(cons_count == 0)
	{
		up(&mtx);
		return -EPIPE;	/* Broken Pipe */
	}
		/* INSERTION */
	kfifo_in(&cbuffer, kbuf, len);

	/* Wake up possible locked consumer -> COND_SIGNAL */
/*********** COND_SIGNAL(PROD, MTX) ***********/
		/* Waits untill kfifo is free (there might be cons) */
	if (nr_cons_waiting > 0)
	{							
		up(&sem_cons);	/* items++ 'cause now there are 1+item in buffer */ 
			/* Wakes up 1 locked thread */
		nr_cons_waiting--;
	}
		/* Mutex Unlocked */
	up(&mtx);
		/* Ret_Value = [written_bytes, ERROR ] */
	return len;
}

static ssize_t fifoproc_read(struct file *file, char __user *buf, size_t len, loff_t *off)
{	
	char kbuf[MAX_KBUF];
	
	if(down_interruptible(&mtx))
		return -EINTR;
	

//COND_WAIT
	while( (kfifo_len(&cbuffer) < len) && (prod_count > 0)){
		/* while reading is not posible, unlocks the mutex and get locked at sem_cons */

		nr_cons_waiting++;
		up(&mtx);
		if(down_interruptible(&sem_cons)){	/* There is some item -> ERROR */
			/* BACKUP */
			down(&mtx);
			nr_cons_waiting--;
			up(&mtx);
			return -EINTR;
		}
		if(down_interruptible(&mtx)){
			nr_cons_waiting--;
			return -EINTR;
		}
	}

		/* EOC detection */
	if(prod_count == 0 && kfifo_is_empty(&cbuffer))		/* To satisfy last specification */
	{
		up(&mtx);
		return 0;
	}

	kfifo_out(&cbuffer, kbuf, len);

	if(nr_prod_waiting > 0){
		up(&sem_prod);
		nr_prod_waiting--;
	}
	up(&mtx);

	if(copy_to_user(buf, kbuf, len))
		return -EINVAL;

		/* Ret_Value = [written_bytes, ERROR ] */
	return len;
}



static struct file_operations fops = {
	.read = fifoproc_read,
	.write = fifoproc_write,
	.open = fifoproc_open,
	.release = fifoproc_release
};

int init_fifodev_module( void )
{
	int ret = SUCCESS;
	int major, minor;

	if(alloc_chrdev_region(&start, 0, 1, DEVICE_NAME) < 0){
		printk(KERN_INFO "FIFODEV: Can not allocate Chardev region");
		return -ENOMEM;
	} 

	if((chardev_fifo = cdev_alloc()) == NULL){
        printk(KERN_INFO "cdev_alloc() failed ");
        return -ENOMEM;
	}

	cdev_init(chardev_fifo, &fops);

	if(cdev_add(chardev_fifo,start,1)){
		printk(KERN_INFO "FIFODEV: cdev_alloc() failed");
		return -ENOMEM;
	}

	major = MAJOR(start);
	minor = MINOR(start);

	printk(KERN_INFO "I was assigned major number %d. To talk to\n", major);
    printk(KERN_INFO "the driver, create a dev file with\n");
    printk(KERN_INFO "'sudo mknod -m 666 /dev/%s c %d %d'.\n", DEVICE_NAME, major,minor);
    printk(KERN_INFO "Try to cat and echo to the device file.\n");
	printk(KERN_INFO "Remove the device file and module when done.\n");

	ret = kfifo_alloc(&cbuffer, MAX_KBUF*sizeof(int), GFP_KERNEL);

	if(ret)
		return -ENOMEM;

		/* Semaphore inicialization -> Mutual Exclusion */
	sema_init(&mtx, 1);

		/* Semaphore inicialization -> Wait Queue */
	sema_init(&sem_prod, 0);
	sema_init(&sem_cons, 0);

		/* Counter inicialization */
	nr_prod_waiting = 0;
	nr_cons_waiting = 0;
	cons_count = 0;
	prod_count = 0;
		/* Ret_Value = [0,Error] */
	return ret;
}

void exit_fifodev_module( void )
{	
	if(chardev_fifo)
		/* Destroy chardev */
		cdev_del(chardev_fifo);
		unregister_chrdev_region(start, 1);
		/* Cleanups kfifo */
	kfifo_reset(&cbuffer);
		/* Frees kfifo allocated memory */
	kfifo_free(&cbuffer);
	printk(KERN_INFO "FIFODEV: module unloaded. Bye kernel\n");
	printk(KERN_INFO "FIFODEV: remember to remove chardev file");
	printk(KERN_INFO "FIFODEV: sudo rm -r /dev/%s", DEVICE_NAME);
}


module_init( init_fifodev_module );
module_exit( exit_fifodev_module );
