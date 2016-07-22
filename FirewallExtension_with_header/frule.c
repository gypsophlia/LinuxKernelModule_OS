#include <linux/module.h>       /* Needed by all modules */
#include "frule.h"
MODULE_AUTHOR ("ZITAI CHEN <chenzitai@139.com>");
MODULE_DESCRIPTION ("Extensions to the firewall") ;
MODULE_LICENSE("GPL");
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
