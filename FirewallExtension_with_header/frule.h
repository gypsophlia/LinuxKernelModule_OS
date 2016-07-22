#include <linux/slab.h>
#include <linux/list.h>         //For using linklist
#define BUFFERSIZE 150


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
int rule_clean(struct list_head *rule);
int rule_init(struct list_head *rule);
int rule_check_f(const struct rule_node *rule,char *path);
struct rule_node *rule_check_p(const struct list_head *rule, int port);
int rule_disply(struct list_head *rule);
int rule_add(struct list_head *rule, int port, char path[BUFFERSIZE]);
