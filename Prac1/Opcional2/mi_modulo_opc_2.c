#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>
#include <linux/list.h>
#include <linux/seq_file.h>
#include <linux/ftrace.h>

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Modlist Kernel Module - FDI-UCM");
MODULE_AUTHOR("Jaime Saez de Buruaga");

#define BUFFER_LENGTH       100

#ifdef PARTE_OPCIONAL
static struct list_item{
	char *data;
	struct list_head links;
};
#else
/*Nodos de la lista*/
static struct list_item{
	int data;
	struct list_head links;
};	
/*Lista enlazada*/
#endif

static struct list_head myList;
static int    myList_counter;		// Contador de elementos de la lista
static int 	  myList_read;			// Contador de elementos leidos de la lista

static struct proc_dir_entry *proc_entry;


static void *modlist_start(struct seq_file *s, loff_t *pos)
{
/*
Takes a position as an argument and returns an iterator which will start reading at that position
*/
	struct list_head *spos = NULL;
	trace_printk("START: pos = %Ld, leidos = %d, ficheros = %d \n", (long long int)pos, myList_read, myList_counter);
	if(myList_counter >= BUFFER_LENGTH)
		return NULL;
	else{
		if(myList_read < myList_counter){
			trace_printk("START_ELSE_IF_WORKS");
		}
		spos = myList.next;
		return spos;
	}	
}
static void *modlist_next(struct seq_file *s, void *v, loff_t *pos)
{
	struct list_head *cur_node = NULL;
	trace_printk("NEXT: leidos = %d, datos = %d \n", myList_read, myList_counter);
	if(myList_read < (myList_counter - 1)){
		cur_node = (struct list_head*) v;
		trace_printk("modlist_next: leidos = %d \n", myList_read);
		myList_read++;
		return cur_node->next;
	}
	else{
		// A NULL pointer indicates EOF
		trace_printk("Reinicio contador de leidos\n");
		cur_node = (struct list_head*) v;
		myList_read = 0;
		return 0;
	}
	
}

static int modlist_show(struct seq_file *s, void *v)
{
	
        //mostrar la lista: ¿seq_printf?
	/* the show() function should format the object currently pointed to by the iterator 
	for output. It should return zero, or an error code if something goes wrong. */
	struct list_head *spos = (struct list_head*) v;
	struct list_item* item=list_entry(spos,struct list_item,links);
	trace_printk("SHOW: dato = %d leidos = %d \n", item->data, myList_read);
	seq_printf(s, "%d\n", item->data);
	return 0;
}
static struct seq_operations modlist_op = {
        .start =        modlist_start,
        .next =         modlist_next,
        .show =         modlist_show
};


static int modlist_open(struct inode *inode, struct file *file)
{
	/* Devolver 0 para indicar una carga correcta del módulo */
	trace_printk("OPEN: leidos = %d, ficheros = %d n", myList_read, myList_counter);
    return seq_open(file, &modlist_op);
}

void add_to_list(int dato){
	struct list_item *item = NULL;	

	item = (struct list_item *)vmalloc(sizeof(struct list_item));
	item->data = dato;
	list_add_tail(&item->links, &myList);
	myList_counter++;
	printk(KERN_INFO "Modlist : item %d added to myList\n", dato);
}

/*PARTE_OBL*/
void cleanup_list( void ){
	struct list_item *item = NULL;
	struct list_head *cur_node = NULL;
	struct list_head *aux = NULL;

	list_for_each_safe(cur_node, aux, &myList){
		item = list_entry(cur_node, struct list_item, links);
		list_del(cur_node);
		vfree(item);
		myList_counter--;
	}
	printk(KERN_INFO "Modlist : myList removed");
}

/*PARTE_OBL*/
void remove_from_list(int dato){
	struct list_item *item = NULL;
	struct list_head *cur_node = NULL;
	struct list_head *aux = NULL;
	
	int found = 0;

	list_for_each_safe(cur_node, aux, &myList){
		item = list_entry(cur_node, struct list_item, links);
		if(item->data == dato){
			vfree(&item->data);
			list_del(cur_node);
			vfree(item);
			myList_counter--;
			found++;		// Actualizo BOOL
		}
	}
	if(found == 0) printk(KERN_INFO "Modlist : item %d not found in myList", dato);
	else printk(KERN_INFO "Modlist : item %d removed from myList", dato);
}


static ssize_t modlist_write(struct file *filp, const char __user *buf, size_t len, loff_t *off) {
	int available_space = BUFFER_LENGTH - 1;
	int num;
	char kbuf[BUFFER_LENGTH];

  	
	
	if((*off) > 0) 
		return 0;

	if(len > available_space) {
		printk(KERN_INFO "modlist: not enough space");
		return -ENOSPC;	// Not Enough Space
	}

	/*Transfer data from user to kernel space*/
	if (copy_from_user( kbuf, buf, len))  
		return -EFAULT;
	
	kbuf[len] = '\0';
	*off+=len;

	// sscanf() return : nº de elementos asignados
	if(sscanf(kbuf, "add %d", &num) == 1){
		add_to_list(num);
	}
	else if(sscanf(kbuf, "remove %d", &num) == 1){
		remove_from_list(num);
	}
	/*strcmp returns 0 if kbuf == "cleanup\n"*/
	else if(strcmp(kbuf, "cleanup\n") == 0){
		cleanup_list();	/*cleans myList*/	
	}
	else{
		printk(KERN_INFO "ERROR: comando no valido\n");
	}
	trace_printk("modlist_write: dato %d modificado, leidos = %d, ficheros = %d \n", num, myList_read, myList_counter);
	return len;
	/*returns the number of bytes written*/
}

static struct file_operations proc_modlist_operations = {
        .open           = modlist_open,
        .read           = seq_read,
        .llseek         = seq_lseek,
        .release        = seq_release,
        .write = modlist_write,    

};

int init_modlist_module( void )
{
	int ret = 0;
	INIT_LIST_HEAD(&myList);	/*Initialize myList*/

	/*create /proc entry for modlist*/
	proc_entry = proc_create( "modlist", 0666, NULL, &proc_modlist_operations);
	if (proc_entry == NULL) {
      		ret = -ENOMEM;	/*NotEnoughMemory*/
      		printk(KERN_INFO "Modlist: Can't create /proc entry\n");
    	}else {
      		printk(KERN_INFO "Modlist: Module loaded\n");
    	}

	/* Devolver 0 para indicar una carga correcta del módulo */
	return ret;

}

void exit_modlist_module( void )
{
	cleanup_list();
  	remove_proc_entry("modlist", NULL);
	printk(KERN_INFO "Modulo LIN descargado. Adios kernel.\n");
}


module_init( init_modlist_module );
module_exit( exit_modlist_module );
