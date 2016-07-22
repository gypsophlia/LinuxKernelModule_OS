/*
 * charDeviceDriver.c: a device driver for a character device  
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/kfifo.h>	/*header for kfifo*/
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/sched.h>	/*for sleep and weak up*/
#include <asm/uaccess.h>	/* for put_user */
#include "ioctl.h"

#include "charDeviceDriver.h"

MODULE_LICENSE("GPL");


DEFINE_MUTEX  (ioLock);	   //For the data completeation of the maxsize
DEFINE_MUTEX  (liLock);    //This lock is for prevent parrallel modify the list when there is only one element in the list(queue)
DECLARE_WAIT_QUEUE_HEAD (rdWaitQ);
DECLARE_WAIT_QUEUE_HEAD (wrWaitQ);
static long device_ioctl(struct file *file,	/* see include/linux/fs.h */
		 unsigned int ioctl_cmd,	/* cmd number and param for ioctl */
		 unsigned long ioctl_param)
{

	mutex_lock (&ioLock);
	if(ioctl_exe)
	{
		mutex_unlock(&ioLock);
		return -EAGAIN;
	}
	ioctl_exe ++;
	mutex_unlock(&ioLock);

	if(ioctl_cmd == CHANGE_MAX_SIZE){
		//Expand the buff
		if(ioctl_param <= msgq_max_size)//input check
		{
			ioctl_exe --;
			return -EINVAL;
		}
	
		//Do Expand
		msgq_max_size = ioctl_param;
		printk(KERN_INFO "Max size changed to %d\n",msgq_max_size); 
	}
	else{
		return -EINVAL;
	}
	mutex_lock(&ioLock);
	ioctl_exe --;
	mutex_unlock(&ioLock);
	return 0;
}


/*
 * This function is called when the module is loaded
 */
int init_module(void)
{
        Major = register_chrdev(0, DEVICE_NAME, &fops);

	if (Major < 0) {
	  printk(KERN_ALERT "Registering char device failed with %d\n", Major);
	  return Major;
	}

	printk(KERN_INFO "I was assigned major number %d. To talk to\n", Major);
	printk(KERN_INFO "the driver, create a dev file with\n");
	printk(KERN_INFO "'mknod /dev/%s c %d 0'.\n", DEVICE_NAME, Major);

	INIT_LIST_HEAD(&msgq);

	return SUCCESS;
}

/*
 * This function is called when the module is unloaded
 */
void cleanup_module(void)
{
	//free the memory
	struct li_node* p;
	//check if the list is empty
	while(!list_empty(&msgq))
	{
		//Get the first element of the queue
		p = list_entry (msgq.next, struct li_node, list);
		kfree(p->msg);			//free the message
		list_del(msgq.next);	//remove the node from the list
		kfree(p);				//free the node
	}

	/*  Unregister the device */
	unregister_chrdev(Major, DEVICE_NAME);

}

/*
 * Methods
 */

/* 
 * Called when a process tries to open the device file, like
 * "cat /dev/mycharfile"
 */
static int device_open(struct inode *inode, struct file *file)
{
	//Allow mutiple threat to open this device
	printk(KERN_INFO "opsysmem device opened!\n"); 
    try_module_get(THIS_MODULE);
    
    return SUCCESS;
}

/* Called when a process closes the device file. */
static int device_release(struct inode *inode, struct file *file)
{
	printk(KERN_INFO "opsysmem device closed!\n"); 
	module_put(THIS_MODULE);
	return SUCCESS;
}

/* 
 * Called when a process, which already opened the dev file, attempts to
 * read from it.
 */
static ssize_t device_read(struct file *filp,	
			   char *buffer,	/* buffer to fill with data */
			   size_t length,	/* length of the buffer     */
			   loff_t * offset)
{
	struct li_node *p;
	int left ;				//the char left to be sent
	int bytes_read = 0;     //num of char already read
	char *msg_Ptr;
	int res;
	//The wait Q
	spin_lock(&rdWaitQ.lock);
	while(Device_read || list_empty(&msgq))
	{
		res = wait_event_interruptible_locked(rdWaitQ, (!Device_read) && (!list_empty(&msgq)));
		if(res == -ERESTARTSYS)
		{
			spin_unlock(&rdWaitQ.lock);
			return -EAGAIN;
		}
	}
	//The Device_read Must be 0 before this stage
	Device_read = 1;
	spin_unlock (&rdWaitQ.lock);


	//Get the first element of the queue
	p = list_entry (msgq.next, struct li_node, list);
	left = p->msglen;	
	//put msg to user
	msg_Ptr  = p -> msg;   //pointer to the msg

	msgq_size -= p->msglen;//Decrease the size counter

	while (length && left){ 
		put_user(*(msg_Ptr++), buffer++);
		length--;
		left--;
		bytes_read++;
	}

	//release the memory
	kfree(p->msg);			//free the message

	//list lock
	mutex_lock(&liLock);
	list_del(msgq.next);	//remove the node from the list
	mutex_unlock(&liLock);

	kfree(p);				//free the node

	spin_lock(&rdWaitQ.lock);
	Device_read = 0;
	wake_up_locked(&rdWaitQ);
	wake_up_locked(&wrWaitQ);
	spin_unlock(&rdWaitQ.lock);

	printk(KERN_INFO "Current size :%d\n",msgq_size);
	return bytes_read;
}

/* Called when a process writes to dev file: echo "hi" > /dev/hello  */
static ssize_t
device_write(struct file *filp, const char *buff, size_t len, loff_t * off)
{

	//check the length of input
	struct li_node *p;
	int bytes_write = 0;    //num of char already write
	struct li_node *temp ;
	char *msg_Ptr;
	int res;
	printk(KERN_INFO "write buff size :%d\n",(int)len);
	//Do not allaw empty string
	if(len == 1 && *buff == '\n' )
	  return -EINVAL;
	if(len > 4096)
	  return -EINVAL;
	
	//The Wait Q
	spin_lock(&wrWaitQ.lock);
	while(Device_write || ((msgq_size + len) > msgq_max_size)){
		res = wait_event_interruptible_locked(wrWaitQ, (!Device_write)&& ((msgq_size + len) <= msgq_max_size)) ;
		if(res == -ERESTARTSYS){
			spin_unlock(&wrWaitQ.lock);
			return -EINVAL;
		}
	}
	Device_write = 1;
	spin_unlock(&wrWaitQ.lock);
	//add node
	p = kmalloc(sizeof(struct li_node), GFP_KERNEL);
	if(!p)
	{
		printk(KERN_ALERT "Memory allcation failed!\n");
		Device_write = 0;
		return -EAGAIN;
	}
	p->msg = kmalloc(len, GFP_KERNEL);
	if(!p->msg)
	{
		kfree(p);
		printk(KERN_ALERT "Memory allocation failed!\n");
		Device_write = 0;
		return -EAGAIN;
	}

	msgq_size += len;

	//get msg from user
	p->msglen = len; 
	msg_Ptr  = p -> msg;   //pointer to the msg

	while (bytes_write < len) {
		get_user(*(msg_Ptr+bytes_write), buff+bytes_write);
		bytes_write ++;
	}
	printk(KERN_INFO "Message read and saved!\nCurrent size :%d\n",msgq_size);


	//Add node to the tail of the list
	mutex_lock(&liLock);
	temp = list_last_entry(msgq.next,struct li_node,list);
	list_add_tail(&(p->list),&(temp->list));
	mutex_unlock(&liLock);

	spin_lock(&wrWaitQ.lock);
	Device_write = 0;
	wake_up_locked(&wrWaitQ);
	wake_up_locked(&rdWaitQ);
	spin_unlock(&wrWaitQ.lock);

	return bytes_write;
}
