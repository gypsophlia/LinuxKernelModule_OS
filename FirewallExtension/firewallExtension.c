#include <linux/module.h>       /* Needed by all modules */
#include <linux/kernel.h>       /* Needed for KERN_ALERT */
#include <linux/proc_fs.h>      //For using proc file
#include <linux/netfilter.h> 
#include <linux/netfilter_ipv4.h>
#include <linux/skbuff.h>
#include <linux/compiler.h>
#include <linux/list.h>         //For using linklist
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <net/tcp.h>
#include <linux/namei.h>
#include <linux/sched.h>        //For get path
#include <linux/dcache.h>       //Get full path

MODULE_AUTHOR ("ZITAI CHEN <chenzitai@139.com>");
MODULE_DESCRIPTION ("Extensions to the firewall") ;
MODULE_LICENSE("GPL");
/* Note:
   client app -> sockets -> TCP&IP -> firewall -> device Driver ->Internet
   the firs packet of tcp protacl is a sim? packet
   without this it will refuse the connection

   Use telnet www.ddd.com 80 to test
   */

/* make IP4-addresses readable */

#define BUFFERSIZE 150
#define CMD_LIST    'L'
#define CMD_WRITE   'W'
#define PROC_ENTRY_FILENAME "firewallExtension"

#define NIPQUAD(addr) \
    ((unsigned char *)&addr)[0], \
((unsigned char *)&addr)[1], \
((unsigned char *)&addr)[2], \
((unsigned char *)&addr)[3]


//**************************/
//rule control
struct rule_node{
    int port;
    struct list_head path;
    struct list_head link;
};
struct path_node{
    char path[BUFFERSIZE];
    struct list_head link;
};
int rule_clean(struct list_head *rule)
{
    struct rule_node *p;   
    struct path_node *pp;
    while(! list_empty(rule)){
        p = list_entry(rule->next, struct rule_node, link);
        //free all the memory of path node
        while(!list_empty( &(p->path) )){
            pp = list_entry((p->path).next, struct path_node, link);//get first node
            list_del((p->path).next);       //remove the first node
            kfree(pp);                      //free the first node
        }
        //free the memory of this rule node
        list_del(rule->next);
        kfree(p);
    }
    return 0;
}
int rule_init(struct list_head *rule)
{
    INIT_LIST_HEAD(rule);
    return 0;
}
int rule_check_f(const struct rule_node *rule,char *path)
{
    struct path_node *pnp;
    if (list_empty( &(rule->path) ))
        return 0;
    pnp = list_entry( (rule->path).next, struct path_node, link);//get the first filename
    while(1)
    {
        if(strcmp(pnp->path, path) == 0)
            return 1;//true
        if( list_is_last(&(pnp->link), &(rule->path)) )//walk into the last node exit loop
            break;
        pnp = list_entry( (pnp->link).next, struct path_node, link);
    }
    return 0; //flase
}
struct rule_node *rule_check_p(const struct list_head *rule, int port)
{
    struct rule_node *pnr;
    if(list_empty(rule))
    {
        return NULL;
    }
    pnr = list_entry(rule->next, struct rule_node, link);
    while (1){
        if(pnr->port == port)
            return pnr;
        if(list_is_last(&(pnr->link),rule))//walk into the last node exit loop
            break;
        pnr = list_entry((pnr->link).next, struct rule_node, link);
    }
    return NULL;
}
int rule_disply(struct list_head *rule)
{
    struct rule_node *pnr;
    struct path_node *pnp;
    if(list_empty(rule))
        return -1;
    //walk through the list and get the information
    pnr = list_entry(rule->next, struct rule_node, link);
    while(1){
        pnp = list_entry((pnr->path).next, struct path_node, link);
        while(1){
            printk ( KERN_INFO "Firewall rule: %d %s\n", pnr->port, pnp->path);
            if(list_is_last(&(pnp->link), &(pnr->path)))//this node is the last node, print and exit loop
                break;
            pnp = list_entry((pnp->link).next, struct path_node, link);
        }
        if(list_is_last(&(pnr->link),rule))//this node is the last node, print and exit loop
            break;
        pnr = list_entry((pnr->link).next, struct rule_node, link);
    }
    return 0;

} 
int rule_add(struct list_head *rule, int port, char path[BUFFERSIZE])
{
    struct rule_node *pnr;
    struct path_node *pnp;
    //check if there is an existing port in the list, if not create a new one.
    pnr = rule_check_p(rule,port);
    if(pnr == NULL){//not exist then create

        pnr = kmalloc(sizeof(struct rule_node), GFP_KERNEL);
        if(pnr == NULL){
            printk(KERN_ALERT "Memory Allocate failed\n");
            return -1;
        }
        INIT_LIST_HEAD(&(pnr->path));
        pnr->port = port;
        list_add_tail(&(pnr->link),rule);
    }
    //Add path to it
    pnp = kmalloc(sizeof(struct path_node), GFP_KERNEL);
    if(pnp == NULL){
        printk(KERN_ALERT "Memory Allocate failed\n");
        //do some cleaning
        if(list_empty(&(pnr->path))){
            list_del(&(pnr->link));
            kfree(pnr);
        }
        return -1;
    }

    strncpy(pnp->path, path, BUFFERSIZE);
    list_add_tail(&(pnp->link), &(pnr->path));
    return 0;
}

/******************************/

//****************************/
//Operations for firewall
DECLARE_RWSEM(rulerw_sem);
DEFINE_MUTEX(w_lock);
static int num_wr = 0;
static LIST_HEAD(rule);
static LIST_HEAD(t_rule);
char rule_cmd;      //rule operation command
static int t_errno; //error number for rule update
struct proc_dir_entry *proc_entry;

unsigned int FirewallExtensionHook (const struct nf_hook_ops *ops,
        struct sk_buff *skb,
        const struct net_device *in,
        const struct net_device *out,
        int (*okfn)(struct sk_buff *)) {

    struct tcphdr *tcp;
    struct tcphdr _tcph;
    struct sock *sk;
    int res;//return value of findExecutable
    struct path path;
    char str_path[BUFFERSIZE];
    char cmdlineFile[BUFFERSIZE];
    char *ppath;
    struct rule_node *prule;

    /*struct dentry *procDentry;
      struct dentry *procDentry;
      struct dentry *parent;*/
    //******************
    //filter begin
    sk = skb->sk;   //get socket
    //Empty socket accept
    if (!sk) {
        printk (KERN_INFO "firewall: netfilter called with empty socket!\n");;
        return NF_ACCEPT;
    }
    //Not TCP packet accept
    if (sk->sk_protocol != IPPROTO_TCP) {
        printk (KERN_INFO "firewall: netfilter called with non-TCP-packet.\n");
        return NF_ACCEPT;
    }
    /* get the tcp-header for the packet */
    tcp = skb_header_pointer(skb, ip_hdrlen(skb), sizeof(struct tcphdr), &_tcph);
    if (!tcp) {
        printk (KERN_INFO "Could not get tcp-header!\n");
        return NF_ACCEPT;
    }
    //get SYN packet
    if (tcp->syn) {
        struct iphdr *ip;

        printk (KERN_INFO "firewall: Starting connection \n");
        ip = ip_hdr (skb);//get ip header
        if (!ip) {
            printk (KERN_INFO "firewall: Cannot get IP header!\n!");
        }
        else {
            printk (KERN_INFO "firewall: Destination address = %u.%u.%u.%u\n", NIPQUAD(ip->daddr));
        }
        printk (KERN_INFO "firewall: destination port = %d\n", ntohs(tcp->dest)); 

        if (in_irq() || in_softirq()) {
            printk (KERN_INFO "Not in user context - retry packet\n");
            return NF_ACCEPT;
        }


        //check if in the list
        //***lock read here
        down_read(&rulerw_sem);
        //Note: tohs is for convert network number to the computer number
        prule = rule_check_p(&rule, ntohs(tcp->dest)); //check port int the rule
        if (prule != NULL) {
            printk (KERN_INFO "Port num in list\n");
            //get the path of the process
            snprintf (cmdlineFile, BUFFERSIZE, "/proc/%d/exe", current->pid); 
            res = kern_path (cmdlineFile, LOOKUP_FOLLOW, &path);
            if (res) {
                //***unlock read
                up_read(&rulerw_sem);
                printk (KERN_INFO "Could not get dentry for %s!\n", cmdlineFile);
                return -EFAULT;
            }
            //get full path 
            ppath = d_path(&path,str_path,BUFFERSIZE);

            /*******************
            // get full path (reversed)
            procDentry = path.dentry;
            while(procDentry!=procDentry->d_parent)
            {
            strncat(rev_buf, procDentry->d_name.name, BUFFERSIZE-strlen(str_path));
            strncat(rev_buf, "\\", BUFFERSIZE-strlen(str_path));
            procDentry = procDentry->d_parent;
            }
             ********************/

            //block the program if it is not in the list (White list)
            if(rule_check_f(prule,ppath)==0){
                //***unlock read
                up_read(&rulerw_sem);
                tcp_done (sk); /* terminate connection immediately */
                printk (KERN_INFO "Connection shut down\n");
                printk (KERN_INFO "PATH:%s\n", ppath);
                return NF_DROP;
            }
        }
        //***unlock read
        up_read(&rulerw_sem);
    }
    return NF_ACCEPT;	
}

EXPORT_SYMBOL (FirewallExtensionHook);

static struct nf_hook_ops firewallExtension_ops = {
    .hook    = FirewallExtensionHook,
    .owner   = THIS_MODULE,
    .pf      = PF_INET,
    .priority = NF_IP_PRI_FIRST,
    .hooknum = NF_INET_LOCAL_OUT
};
//**************************/

/**********************************/
//Operations for proc file
ssize_t kernelWrite (struct file *file, const char __user *buffer, size_t count, loff_t *offset) {
    int i,ii;
    int port;
    char *temp;             //used for store data from user
    char fname[BUFFERSIZE];
    int res;
    printk (KERN_INFO "kernelWrite entered\n");
    //get userspace date
    temp = kmalloc(count+1,GFP_KERNEL);
    if (temp == NULL) {
        printk (KERN_INFO "Memallocate failed!\n");
        t_errno = -1;
        return 0;
    }
    //for(i =0; i<count; i++){
    //  snprintf(str_path,BUFFERSIZE-strlen(str_path),"/%s",procDentry->d_name.name);
    //    get_user(temp[i],buffer[i]);
    //}
    if(copy_from_user(temp,buffer,count))
    {
        printk(KERN_INFO "copy from user failed\n");
        kfree(temp);
        t_errno = -1;
        return -EFAULT;
    }
    temp[count] = '\0';
    //get command L or W
    rule_cmd = temp[0];
    switch(rule_cmd){
        case CMD_LIST:
            down_read(&rulerw_sem);
            rule_disply(&rule);
            up_read(&rulerw_sem);
            break;
        case CMD_WRITE:
            //get port
            for(i = 2, ii=0; i<count; i++, ii++ ){
                fname[ii] = temp[i];//temperaturaly use fname to store port string
                if(fname[ii] == ' '){
                    fname[ii] = '\0';
                    break;
                }
            }
            res = kstrtoint( fname,10,&port );//convert port string to int decimal(10)
            if(res != 0){
                t_errno = -1;
                kfree(temp);
                printk(KERN_INFO "Port number convertion failed\n");
                return 0;
            }

            //get path
            i++;//skip the space
            strncpy(fname,temp+i,BUFFERSIZE);
            //add one rule
            res = rule_add(&t_rule, port, fname);
            if(res != 0)
                t_errno = -1;
            break;
        default:
            printk (KERN_INFO "firewallExtension: Illegal command \n" );
    }
    //kfree(temp);

    return count;
}

int procfs_open(struct inode *inode, struct file *file)
{
    //***All mutex to lock serveral open
    mutex_lock(&w_lock);
    if(num_wr != 0){
        mutex_unlock(&w_lock);
        return -EAGAIN;
    }
    num_wr = 1;
    mutex_unlock(&w_lock);

    printk (KERN_INFO "/proc/firewallExtension opened\n");
    try_module_get(THIS_MODULE);
    rule_init(&t_rule);
    t_errno = 0;
    rule_cmd = 'O';//default cmd
    return 0;
}

int procfs_close(struct inode *inode, struct file *file)
{
    if(rule_cmd == CMD_WRITE)
    {
        if(t_errno == 0){
            //***Lock rule write
            down_write(&rulerw_sem);
            rule_clean(&rule);
            list_replace(&t_rule, &rule);
            //***Unlock rule write
            up_write(&rulerw_sem);
            printk(KERN_INFO "Rule update succeed!\n");
        }
        else{
            printk(KERN_INFO "Rule update failed!\n");
            rule_clean(&t_rule);
        }
    }

    printk (KERN_INFO "/proc/firewallExtension closed\n");
    module_put(THIS_MODULE);
    //***Unlock open
    mutex_lock(&w_lock);
    num_wr = 0;
    mutex_unlock(&w_lock);
    return 0;		/* success */
}

const struct file_operations File_Ops_4_Our_Proc_File = {
    .owner = THIS_MODULE,
    .write 	 = kernelWrite,
    .open 	 = procfs_open,
    .release = procfs_close,
};
/*********************************/

int init_module(void)
{

    int errno;

    errno = nf_register_hook (&firewallExtension_ops); /* register the hook */
    if (errno) {
        printk (KERN_INFO "Firewall extension could not be registered!\n");
    } 
    else {
        printk(KERN_INFO "Firewall extensions module loaded\n");
    }

    /* create the /proc file */
    proc_entry = proc_create_data (PROC_ENTRY_FILENAME, 0644, NULL, &File_Ops_4_Our_Proc_File, NULL);

    /* check if the /proc file was created successfuly */
    if (proc_entry == NULL){
        printk(KERN_ALERT "Error: Could not initialize /proc/%s\n",PROC_ENTRY_FILENAME);
        return -ENOMEM;
    }

    printk(KERN_INFO "/proc/%s created\n", PROC_ENTRY_FILENAME);


    rule_init(&rule);

    // A non 0 return means init_module failed; module can't be loaded.
    return errno;
}



void cleanup_module(void)
{

    down_write(&rulerw_sem);
    rule_clean(&rule);
    up_write(&rulerw_sem);
    
    rule_clean(&t_rule);
    nf_unregister_hook (&firewallExtension_ops); /* restore everything to normal */
    printk(KERN_INFO "Firewall extensions module unloaded\n");

    remove_proc_entry(PROC_ENTRY_FILENAME, NULL);
    printk(KERN_INFO "/proc/%s removed\n", PROC_ENTRY_FILENAME);
}  
