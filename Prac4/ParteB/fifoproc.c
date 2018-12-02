#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kfifo.h>		
#include <linux/semaphore.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
	/* /proc Entry */
static struct proc_dir_entry *proc_entry;

	/* Global Variables */
struct kfifo cbuffer;			/* Cyclic buffer */
int prod_count = 0;			/* Number of productor process (write) */
int cons_count = 0;			/* Number of consumer process */
struct semaphore mtx;			/* Mutual exclusion */
struct semaphore sem_prod;		/* Costumers waiting queue -> number of items */
struct semaphore sem_cons;		/* Productors waiting queue -> number of holes */
int nr_prod_waiting = 0;		/* Number of prdocutors waiting */
int nr_cons_waiting = 0;		/* Number of consumers waiting */
	/* GLobal Constants */
#define MAX_KBUF				256		/* Max tam kernel buffer */
#define MAX_CBUFFER_LEN 		256		/* Max tam cyclic buffer */ 


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
		printk(KERN_INFO "LECTOR SE VA, prod_count = %d, cons_count = %d", prod_count, cons_count);

	} else {

		prod_count--;
			/* Desbloqueo posible consumidor bloqueado por espacio */
		if(nr_cons_waiting > 0){
			nr_cons_waiting--;
			up(&sem_cons);
		}
		printk(KERN_INFO "PRODUCTOR SE VA, prod_count = %d, cons_count = %d", prod_count, cons_count);

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

/************************** /proc entry functions ***********************/

static const struct file_operations proc_entry_fops = {
	.open = fifoproc_open,
	.release = fifoproc_release,
	.read = fifoproc_read,
	.write = fifoproc_write,
};

int init_fifoproc_module( void )
{
	int ret = 0;

	proc_entry = proc_create_data("fifoproc", 0666, NULL, &proc_entry_fops, NULL);	/* r+w for all */
	if(proc_entry == NULL)
	{
		printk(KERN_INFO "FIFOPROC: Cannot allocate mamory for /proc entry\n");
		return -ENOMEM; /* Not Enough Memory */
		
	} else{
		printk(KERN_INFO "FIFOPROC: Module loaded\n");
	}
		/* Memory allocation: 2⁵ on GFP_KERNEL mode */
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

void exit_fifoproc_module( void )
{		/* Cleanups kfifo */
	kfifo_reset(&cbuffer);
		/* Frees kfifo allocated memory */
	kfifo_free(&cbuffer);
	remove_proc_entry("fifoproc", NULL);
	printk(KERN_INFO "FIFOPROC module unloaded. Bye kernel\n");
}


module_init( init_fifoproc_module );
module_exit( exit_fifoproc_module );
