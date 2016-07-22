#include <linux/kernel.h>       /* Needed for KERN_ALERT */
#include <linux/module.h>
#include <linux/namei.h>        //For getting the path of the <path>
#include <linux/fs.h>           //for update dirty inodes
#include <linux/bitops.h>       //for bitmap operation
#include <linux/buffer_head.h>
#include "minix.h"
/* TODO: Implement three functions, namely minix_chown, process_inodes and proc_chown */


/* minix_chown: 
 * sets uid-field for given inode to specified value
 */
int minix_chown(unsigned int uid, unsigned long ino, struct super_block *sb)
{
    struct inode *inode;

    //printk(KERN_INFO "Enter chown()\n");

    inode = minix_iget(sb,ino);
    if(inode == ERR_PTR(-ENOMEM) || inode == ERR_PTR(-EIO)){
        printk(KERN_INFO "Get inode %lu failed\n", ino);
        return -EFAULT;
    }

    (inode->i_uid).val = uid; 

    mark_inode_dirty(inode);
    iput(inode);
    return 0;
}
/* process_inodes: 
 *  goes through the map of valid inodes and for each valid inode calls minix_chown
 */

int process_inodes(unsigned int uid, struct super_block *sb)
{
    struct minix_sb_info *sbi;
    struct buffer_head *bh;
    int k = sb->s_blocksize_bits + 3;
    int i;
    __u32 numbits;

    sbi = minix_sb(sb);
    numbits = sbi->s_ninodes + 1;

    //printk (KERN_INFO "Enter process_inodes()\n");
    //printk(KERN_INFO "ninodes:%ld\nnzones:%ld\nimap_size:%ld\n",sbi->s_ninodes,sbi->s_nzones,sbi->s_imap_blocks);

    for(i=1; i<numbits; i++){//ino start with 1 end with numbits
        unsigned long ino = i;
        unsigned long bit;
        bit = ino & ((1<<k) -1);
        ino >>= k;
        bh = sbi->s_imap[ino];
        if(test_bit(bit,(unsigned long*)bh->b_data)){
            if(minix_chown(uid,i,sb) != 0){
                return -EFAULT;
            }
        }
    }

    return 0;
}
/* proc_chown 
 * handles writing to the proc-file. It processes the string passed to it, and calls minix_chown or process_inodes as appropriate.
 */
int proc_chown (char arg[501])
{
    int uid;
    int ino;
    char pname[500];


    struct path path;
    struct dentry *dentry;
    struct super_block *sb;
    int rval;
    /////////////////
    //Check formate
    rval = sscanf (arg, "%d %d %s", &uid, &ino, pname);
    if( rval!=3 || uid<0 || uid >65535 || ino < 0){
        printk(KERN_INFO "Formate Error\n");
        return -EFAULT;
    }

    /////////////////
    //pname string -> path -> dentry -> super block
    //get dentry
    if( kern_path(pname, LOOKUP_FOLLOW, &path)){
        printk (KERN_INFO "File not exitst!\n");
        return -EFAULT;
    }
    dentry = path.dentry; 
    //get super block
    sb = dentry->d_sb;

    /////////////////
    //valid arguements check
    //check if minix fs
    if(strncmp(sb->s_type->name,"minix",5)){
        printk(KERN_INFO "Filesystem of given path is NOT minix\n");
        goto e1;
    }
    //check the <ino> match
    if(ino != 0 && ino != ((path.dentry)->d_inode->i_ino)){
        printk(KERN_INFO "Thie Given <ino> is NOT the ino of <path>\n");
        goto e1;
    } 

    //printk(KERN_INFO "read input uid:%d\nino:%d\npname:%s\n",uid,ino,pname);

    /////////////////
    //do operations
    if(ino == 0){           //change all
        if(process_inodes(uid,sb)){
            printk(KERN_INFO "process_inodes failed\n");
            goto e1;
        }
    }
    else{                   //change the uid of ino to new uid
        if(minix_chown(uid,ino,sb)){
            printk(KERN_INFO "chown failed\n");
            goto e1;
        }
    }

    path_put(&path);
    return 0;

e1: path_put(&path);
    return -EFAULT;
}
