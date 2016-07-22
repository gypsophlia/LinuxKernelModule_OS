/* Global definition for the example character device driver */

int init_module(void);
void cleanup_module(void);
static int device_open(struct inode *, struct file *);
static int device_release(struct inode *, struct file *);
static ssize_t device_read(struct file *, char *, size_t, loff_t *);
static ssize_t device_write(struct file *, const char *, size_t, loff_t *);
static long device_ioctl(struct file *file, unsigned int ioctl_num, unsigned long);

#define SUCCESS 0
#define DEVICE_NAME "opsysmem"	/* Dev name as it appears in /proc/devices   */
#define BUF_LEN 80		/* Max length of the message from the device */

/* 
 * Global variables are declared as static, so are global within the file. 
 */
struct cdev *my_cdev;
       dev_t dev_num;

static int Major;		/* Major number assigned to our device driver */

 /* Used to prevent multiple access to device */
static int Device_read = 0;	/* Is device being read?*/
static int Device_write = 0; /* Is device being write*/
static int ioctl_exe = 0;//if someone is using ioctrl


//Message struct
static LIST_HEAD(msgq);
static int msgq_size = 0;
static int msgq_max_size = 2097152; 
struct li_node{
	char * msg;
	int msglen;
	struct list_head list;	
};


static struct file_operations fops = {
	.read = device_read,
	.write = device_write,
	.open = device_open,
	.unlocked_ioctl = device_ioctl,
	.release = device_release
};
