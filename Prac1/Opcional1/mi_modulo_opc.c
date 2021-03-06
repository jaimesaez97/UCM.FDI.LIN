#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>
#include <linux/list.h>
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Modlist Kernel Module - FDI-UCM");
MODULE_AUTHOR("Jaime Saez de Buruaga");

#define BUFFER_LENGTH       100
#define WORD_LENGTH	    30
static struct proc_dir_entry *proc_entry;

#ifdef PARTE_OPCIONAL
static struct list_item{
	char *data;
	struct list_head links;
};
#else
/*Nodos de la lista*/
static struct list_item {
	int data;
	struct list_head links;
};	
/*Lista enlazada*/
#endif

static struct list_head myList;

#ifdef PARTE_OPCIONAL
/*PARTE_OPC*/
void add_to_list(char* dato){
	struct list_item *item = NULL;
	item = (struct list item*)vmalloc(sizeof(struct list_item));
	item->data = (char*)vmalloc(sizeof(strlen(dato) + 1));
	item->data = dato;
	list_add_tail(&item->links, &myList);
	printk(KERN_INFO "Modlist : item %d added to myList", dato);
}
/*PARTE_OPC*/
void cleanup_list( void ){
	struct list_item *item = NULL;
	struct list_head *cur_node = NULL;
	struct list_head *aux = NULL;

	list_for_each_safe(cur_node, aux, &myList){
		item = list_entry(cur_node, struct list_item, links);
		vfree(item->data);
		list_del(cur_node);
		vfree(item);
	}
	printk(KERN_INFO "Modlist : myList removed");
}
/*PARTE_OPC*/
void remove_from_list(char *dato){
	struct list_item *item = NULL;
	struct list_head *cur_node = NULL;
	struct list_head *aux = NULL;
	
	int found = 0;

	list_for_each_safe(cur_node, aux, &myList){
		item = list_entry(cur_node, struct list_item, links);
		if(item->data == dato){
			list_del(cur_node);
			vfree(item);
			found++;		// Actualizo BOOL
		}
	}
	if(found == 0) printk(KERN_INFO "Modlist : item %s not found in myList", dato);
	else printk(KERN_INFO "Modlist : item %s removed from myList", dato);
}
#else
void add_to_list(int dato){
	struct list_item *item = NULL;	

	item = (struct list_item *)vmalloc(sizeof(struct list_item));
	item->data = dato;
	list_add_tail(&item->links, &myList);

	printk(KERN_INFO "Modlist : item %d added to myList", dato);
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
			found++;		// Actualizo BOOL
		}
	}
	if(found == 0) printk(KERN_INFO "Modlist : item %d not found in myList", dato);
	else printk(KERN_INFO "Modlist : item %d removed from myList", dato);
}

#endif

static ssize_t modlist_write(struct file *filp, const char __user *buf, size_t len, loff_t *off) {
	int available_space = BUFFER_LENGTH - 1;
#ifdef PARTE_OPCIONAL
	char *num;
#else
	int num;
#endif
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

#ifdef PARTE_OPCIONAL
	// sscanf() return : nº de elementos asignados
	if(sscanf(kbuf, "add %s", &num) == 1){
		add_to_list(num);
	}
	else if(sscanf(kbuf, "remove %s", &num) == 1){
		remove_from_list(num);
	}
	/*strcmp returns 0 if kbuf == "cleanup\n"*/
	else if(strcmp(kbuf, "cleanup\n") == 0){
		cleanup_list();	/*cleans myList*/	
	}
	else{
		printk(KERN_INFO "ERROR: comando no valido\n");
	}
#else
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
#endif
	return len;
	/*returns the number of bytes written*/
}

static ssize_t modlist_read(struct file *filp, char __user *buf, size_t len, loff_t *off) {

	int nBytes;
	struct list_item* item = NULL;
  	struct list_head* cur_node = NULL;

	char kbuf[BUFFER_LENGTH] = "";
	char *list_string = kbuf;

	if((*off) > 0) 
		return 0; 

#ifdef PARTE_OPCIONAL	
	list_for_each(cur_node, &myList) {
		/*item points to the structure wherein the links are embedded*/
		item = list_entry(cur_node, struct list_item, links);
		list_string += sprintf(list_string, "%s\n", item->data);
  	}
#else
	list_for_each(cur_node, &myList) {
		/*item points to the structure wherein the links are embedded*/
		item = list_entry(cur_node, struct list_item, links);
		list_string += sprintf(list_string, "%d\n", item->data);
  	} 
#endif
	nBytes = list_string-kbuf; 
	
	if(len < nBytes)
		return -ENOSPC; /*NotEnoughSpace*/
	
	if(copy_to_user(buf, kbuf, nBytes))
		return -EINVAL;	/*NotValidArgument*/

	(*off)+=len;	/*Update the file-pointer*/
	return nBytes;
}

static const struct file_operations proc_entry_fops = {
    .read = modlist_read,
    .write = modlist_write,    
};



int init_modlist_module( void )
{
	int ret = 0;
	INIT_LIST_HEAD(&myList);	/*Initialize myList*/

	/*create /proc entry for modlist*/
	proc_entry = proc_create( "modlist", 0666, NULL, &proc_entry_fops);
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
