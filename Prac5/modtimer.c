/*
	DUDAS:
		1.- ¿NO SE DEBE PROTEGER EMERGENCY_TREESHOLD AL ESCRIBIR EN MODCONF?
		2.- ¿COMO SE BORRA LA CABECERA DE UN STRUCT LIST_HEAD? (list_for_each_entry)
		3.-

*/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/list.h>
#include <linux/timer.h>
#include <linux/string.h>							/* For strlen() */
#include <linux/kfifo.h>
#include <linux/random.h> 							/* For get_random_int() */ 
#include <linux/semaphore.h>
	/* Global Constants */
#define MAX_CONF_BUFF			512					/* To write CONF file */
#define MAX_BYTES_FIFO 			32					/* 4 Int */
#define MAX_LIST_TAM			256					/* Max struct list_head tam */ 
#define SUCCESS					0					/* For return */

MODULE_LICENSE("GPL");
MODULE_AUTHOR("JAIME SAEZ DE BURUAGA");
MODULE_DESCRIPTION("LIN - P5");

	/*------------------------- Global Variables -------------------------*/
struct timer_list my_timer; 						/* Structure that describes the kernel timer */

struct kfifo cbuffer;								/* Cyclic buffer */

static struct proc_dir_entry *proc_conf_entry;
static struct proc_dir_entry *proc_read_entry;
unsigned long timer_period_ms=1000;						/* TIMER period */
unsigned long max_random=25;							/* TIMER max random */
unsigned long emergency_treeshold=80;					/* TIMER emergency treeshold */

static struct workqueue_struct *my_wq; 				/* WORK QUEUE*/
struct work_struct my_work;							/* Work to queue */

DEFINE_SPINLOCK(buffer_sp);							/* SPINLOCK for kfifo */
struct semaphore list_mtx;							/* SEMAPHORE for mutual exclusion on STRUCT LIST_HEAD */
unsigned int list_mtx_counter;						/* # of process waiting at list_mtx */
struct semaphore list_wqueue;						/* SEMAPHORE for LIST_HEAD wait queue*/
unsigned int nr_cons_waiting;						/* # of process waiting at list_wqueue (empty list) */

static struct list_head myList;						/* Linked list */
struct list_item {
	int data;
	struct list_head links;
};													/* List item */


/* PARA PARTE OPCIONAL */
//static struct list_head myList_impares;
//struct semaphore module_wqueue;						/* process waiting to start (max 1) */
//unsigned int nr_proc_waiting;						/* must be 2 to start (par & impar) */


/*------------------------------ IMPLEMENTATION ------------------------------*/
static void add_to_list(int dato){
	struct list_item *item = NULL;
	item = (struct list_item *)vmalloc(sizeof(struct list_item));
	item->data = dato;
	list_add_tail(&item->links, &myList);
	printk(KERN_INFO "MODTIMER : item %d added to myList\n", dato);
}

static int cleanup_list( void ){
	struct list_item *item = NULL;
	struct list_head *cur_node = NULL;
	struct list_head *aux = NULL;

	if(down_interruptible(&list_mtx))
		return -EINTR;

	list_for_each_safe(cur_node, aux, &myList){
		item = list_entry(cur_node, struct list_item, links);
		list_del(cur_node);
		vfree(item);
	}

	up(&list_mtx);

	return SUCCESS;
}

static void copy_items_into_list( void ){
	unsigned long buffer_flags;
	int aux[MAX_BYTES_FIFO/sizeof(int)];
	int n;
	int i=0;

	spin_lock_irqsave(&buffer_sp, buffer_flags);

	n = 0;
	while(!kfifo_is_empty(&cbuffer)){
		kfifo_out(&cbuffer, &aux[n], sizeof(int));
		++n;
	}
	kfifo_reset(&cbuffer); 	//ESPECIFICACION: vacía buffer
	spin_unlock_irqrestore(&buffer_sp, buffer_flags);

	/* No hacemos down(&list_mtx) porque se hace en add_to_list */

		/* Now I've kfifo in aux[] */

	down(&list_mtx);/* Aquí el cerrojo en vez de dentro de add_to_list() */
						    /* para no partir la SECCION CRITICA */

	for (i = 0; i < n; i++){
		add_to_list(aux[i]);	// Si falla es por temas de memoria

		if(nr_cons_waiting > 0){
			nr_cons_waiting--;	// PARA QUE VAYA LEYENDO EL CONSUMIDOR 
								// SE DEBERIA HACER DOWN (MTX), NO?
			up(&list_wqueue);
		}
	}

	up(&list_mtx);
		/* Now I've kfifo on myList */
}

static void my_wq_function(struct work_struct *work){
	copy_items_into_list();
	printk(KERN_INFO "MODTIMER: Se activa tarea de vaciado\n");
}

	/* Function invoked when timer expires (fires) */
static void fire_timer(unsigned long data)
{        
	unsigned long buffer_flags;
	unsigned int random=get_random_int() % max_random;

	INIT_WORK(&my_work, my_wq_function);
	
	spin_lock_irqsave(&buffer_sp, buffer_flags);

	kfifo_in(&cbuffer, &random, sizeof(int));
	printk(KERN_INFO "Insertando %u en el buffer\n", random);

	if(((kfifo_len(&cbuffer) * 100)/ kfifo_size(&cbuffer)) >= emergency_treeshold){

			// ¿HAY AQUI INTERBLOQUEO?
		
		queue_work(my_wq, &my_work);
		/* No se debe planificar la tarea diferida hasta que no se haya
		   completado la ejecución de la última tarea planificada y el
		   umbral de emergencia se vuelva a alcanzar */
	}

	spin_unlock_irqrestore(&buffer_sp, buffer_flags);	// AQUI O ANTES??

        /* Re-activate the timer one second from now */
	mod_timer(&(my_timer), jiffies + ((timer_period_ms/1000)*HZ)); 
}

/*******************************************	 MODTIMER      *******************************************/
static int modtimer_open(struct inode *inode, struct file *file){
	try_module_get(THIS_MODULE);
	add_timer(&my_timer);		// Falla aqui??
	return SUCCESS;
}

static int modtimer_release(struct inode *inode, struct file *file){
		/* TIMER */
	del_timer_sync(&my_timer);
		/* WORK_QUEUE */
	flush_workqueue(my_wq);
		/* KFIFO */
	kfifo_reset(&cbuffer);	
		/* MODULE KREF */
	module_put(THIS_MODULE);
		/* LINKED LIST */
	cleanup_list();	// 0     -> SUCCESS
							// o.w.  -> EINTR
	return 0;
}

static int read_and_remove_from_list(char *kbuf){
	struct list_item *item = NULL;
	struct list_head *cur_node = NULL;
	struct list_head *aux = NULL;
	char *list_string = kbuf;
	int len;

	list_for_each_safe(cur_node, aux, &myList){
		item = list_entry(cur_node, struct list_item, links);
		list_string += sprintf(list_string, "%d\n", item->data);
		list_del(cur_node);
		vfree(item);
	}
		/* Aritmética de punteros */
	len = list_string - kbuf;
	return len; 
}

static ssize_t modtimer_read(struct file *file, char __user *buf, size_t len, loff_t *off){
	char kbuf[MAX_CONF_BUFF];
	int length;

	if((*off) > 0)
		return 0;

	if(down_interruptible(&list_mtx))
		return -EINTR;

	printk(list_empty(&myList) ? "MODTIMER_READ: list_empty = TRUE" : "MODTIMER_READ: list_empty = FALSE");
	while(list_empty(&myList)){
		nr_cons_waiting++;

		up(&list_mtx);

		if(down_interruptible(&list_wqueue)){
			down(&list_mtx);
			nr_cons_waiting--;
			up(&list_mtx);
			return -EINTR;
		}
		if(down_interruptible(&list_mtx)){
			nr_cons_waiting--;
			return -EINTR;
		}
	}

	length = read_and_remove_from_list(kbuf);
	up(&list_mtx);

	if(len < length)
		return -ENOSPC;
		/* ¿Dentro o fuera de MTX? */
	if(copy_to_user(buf, kbuf, length))	
		return -EINVAL;	
	
	return length;
}

static const struct file_operations proc_read_fops = {
	.open = modtimer_open,
	.release = modtimer_release,
	.read = modtimer_read,
};
/*******************************************	 MODCONF      *******************************************/
static ssize_t modconf_read(struct file *file, char __user *buf, size_t len, loff_t *off){
	char kbuf[MAX_CONF_BUFF];
	int length;
	char *listString;

	listString = kbuf;

	if(*(off) > 0)
		return 0;

	length = 0;
	listString += sprintf(listString, "timer_period_ms=%lu\n", timer_period_ms);
	printk(KERN_INFO "timer_period_ms=%lu", timer_period_ms);
	listString += sprintf(listString, "emergency_treeshold=%lu\n", emergency_treeshold);
	printk(KERN_INFO "emergency_treeshold=%lu", emergency_treeshold);
	listString += sprintf(listString, "max_random=%lu\n", max_random);
	printk(KERN_INFO "max_random=%lu", max_random);

		/* Aritmética de punteros */
	length = listString - kbuf;

	if(len < length)	
		return -ENOSPC;	/* NES in buffer */

	printk("MODCONF_READ kbuf = %s", kbuf);
	if(copy_to_user(buf, kbuf, length))
		return -EINVAL;

	// printk("MODCONF_READ buf = %s", buf); <-Aqui falla
	(*off)+=len;
	return length;
}

static ssize_t modconf_write(struct file *file, const char __user *buf, size_t len, loff_t *off){
	int available_space = MAX_CONF_BUFF - 1;
	char kbuf[MAX_CONF_BUFF];
	int length;

	if((*off) > 0)
		return 0;

	printk("MODCONF_WRITE: len = %zu",len);
	if(len  > available_space)
		return -ENOSPC;

	if(copy_from_user(kbuf, buf, len))
		return -EINVAL;

	kbuf[len] = '\0';
	*off+=len;

	length = 0;
	if((length = sscanf(kbuf, "timer_period_ms=%lu", &timer_period_ms)) == 1)
		;
	
	if((length = sscanf(kbuf, "emergency_treeshold=%lu", &emergency_treeshold)) == 1)
		;

	if((length = sscanf(kbuf, "max_random=%lu", &max_random)) == 1)
		;

	return len;	// ¿LEN y no LENGTH?
}

static const struct file_operations proc_conf_fops = {
	.read = modconf_read,
	.write = modconf_write,
};

int init_modtimer_module ( void ){
	int ret = SUCCESS;

		/* PROC_CONF entry*/
	proc_conf_entry = proc_create_data("modconf", 0666, NULL, &proc_conf_fops, NULL);
	if(proc_conf_entry == NULL)
	{
		printk(KERN_INFO "MODCONF: Cannot allocate mamory for /proc configuration entry\n");
		ret = -ENOMEM; /* Not Enough Memory */
		goto out_error;
		
	} 
		/* PROC_READ entry */
	proc_read_entry = proc_create_data("modtimer", 0666, NULL, &proc_read_fops, NULL);
	if(proc_read_entry == NULL)
	{
		printk(KERN_INFO "MODTIMER: Cannot allocate mamory for /proc configuration entry\n");
		ret = -ENOMEM; /* Not Enough Memory */
		goto out_error;
		
	}
		/* KFIFO */
	ret = kfifo_alloc(&cbuffer, MAX_BYTES_FIFO, GFP_KERNEL);
	if(ret)
		goto out_error;

		/* SYNCHRONIZATION */
	sema_init(&list_mtx, 1);		// Mutual exclusion on STRUCT LIST_HEAD
	sema_init(&list_wqueue, 0);		// Waitqueue on STRUCT LIST_HEAD 
		/* WORK_QUEUE */
  	my_wq = create_workqueue("my_queue");
  	if(!my_wq)
  		goto out_error;

  		/* LINKED LIST */
  	INIT_LIST_HEAD(&myList);	/*Initialize myList*/

  		/* TIMER */
  	init_timer(&my_timer);
  	my_timer.data = 0;
  	my_timer.function = fire_timer;
  	my_timer.expires = jiffies + HZ;	// One second from now
  	//add_timer(&my_timer);

	return ret;

out_error:

	if(proc_conf_entry != NULL)
		remove_proc_entry("modconf", NULL);
	if(proc_read_entry != NULL)
		remove_proc_entry("modtimer", NULL);

	/* Frees kfifo allocated memory */
	//kfifo_free(&cbuffer);
	if(my_wq != NULL)
		/* Destroy workqueue resources */
 		destroy_workqueue( my_wq );

	return ret;
}

void exit_modtimer_module( void ){
		/* PROF_CONF entry */
	remove_proc_entry("modconf", NULL);
		/* PROF_READ entry */
	remove_proc_entry("modtimer", NULL);
		/* SYNCHRONIZATION */
	flush_workqueue(my_wq);
	destroy_workqueue( my_wq );
}

module_init( init_modtimer_module );
module_exit( exit_modtimer_module );


