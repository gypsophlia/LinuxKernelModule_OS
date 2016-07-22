#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h> //for read open write close
#include <fcntl.h>  //for read open write close
#include <sys/stat.h>//for stat func
#define BUFFERSIZE 300

#define CMD_LIST    'L'
#define CMD_WRITE   'W'
int main(int argc, char **argv)
{
    FILE *fd = NULL;    //poiter for rule file
    int fp;
    int i;
    char port[6];
    char buff[BUFFERSIZE];
    char path[BUFFERSIZE];
    //char tmp[BUFFERSIZE]; temp string for follow symbolic link
    char msg[BUFFERSIZE+2];
    struct stat fstat;
    //check arguement
    if(argc >3 || argc <2 || ((argv[1][0]!=CMD_LIST) && (argv[1][0]!=CMD_WRITE))){
        printf("Usesage: %s L \nor: %s W <filename>\n",argv[0],argv[0]);
        return 1;
    }

    //List operation
    if(argv[1][0] == CMD_LIST){
        fp = open("/proc/firewallExtension", O_WRONLY);
        if(fp == -1){
            printf("Open /proc/firewallExtension failed, try again!\n");
        }
        if(write(fp, "L", 1) == -1){
            printf("Write /proc/firewallExtension failed.\n");
        }
        close(fp);
    }
    //Firewall rule update operation
    else if(argv[1][0] == CMD_WRITE){
        memset(buff,0,sizeof(buff));
        //open the file
        fd = fopen(argv[2],"r");
        if(NULL == fd){
            printf("rule open Error!!!\n");
            return 1;
        }
        ////////////////////////////////////
        //check the file
        while (NULL != fgets(buff, BUFFERSIZE, fd)) {
            //check syntax error
            //1.check port 
            if(sscanf(buff,"%6s %300s", port, path)==EOF){
                printf("ERROR: Ill-formed file\n");
                fclose(fd);
                return -1;
            }
            for(i=0;i<strlen(port);i++){
                if(port[i]<'0' || port[i]>'9'){
                    printf("ERROR: Ill-formed file\n");
                    fclose(fd);
                    return -1;
                }
            }
            //2.check file path(space is not allowed in the path)
            if(buff[strlen(port)+strlen(path)+1] != '\n'){
                printf("ERROR: Ill-formed file\n");
                fclose(fd);
                return -1;
            }
            //check execuateable
            if(stat(path, &fstat) == -1){//file not exist
                printf("ERROR: Cannot execute file respectively\n");
                fclose(fd);
                return -1;
            }
            //file not execuatable
            if( (fstat.st_mode & S_IXUSR)==0 && (fstat.st_mode & S_IXGRP)==0 && (fstat.st_mode & S_IXOTH)==0 ){
                printf("ERROR: Cannot execute file respectively\n");
                fclose(fd);
                return -1;
            }
        }

        //reset the file pointer
        fseek(fd, 0, SEEK_SET);

        /////////////////////////////////
        //write the file into the kernel
        fp = open("/proc/firewallExtension", O_WRONLY);
        if(fp == -1){
            printf("Open /proc/firewallExtension failed, try again!\n");
            fclose(fd);
            return -1;
        }
        while (NULL != fgets(buff, BUFFERSIZE, fd)) {

            //comment below and the tmp var to not follow symbolic link
            /*if(sscanf(buff,"%6s %300s", port, path)==EOF){
                printf("ERROR: sscanf() failed\n");
                fclose(fd);
                close(fp);
                return -1;
            }
            //check if the path given is a symbolic link
            lstat(path,&fstat);
            if(S_ISLNK(fstat.st_mode) != 0){//it is a link
                strncpy(tmp, path, BUFFERSIZE);
                i = readlink(tmp, path, BUFFERSIZE);//renew path
                path[i] = '\0';
                printf("%s", path);
            }
            sprintf(msg, "W %s %s", port, path);
            if(write(fp, msg, strlen(msg)) == -1){
                printf("Write /proc/firewallExtension failed.\n");
            }*/
            /**************************/
            
            //Uncomment blow and comment the code up to not follow the symbolic link
            sprintf(msg, "W %s", buff);
            if(write(fp, msg, strlen(msg)-1) == -1){//trime /n
                printf("Write /proc/firewallExtension failed.\n");
            }
            /**************************/
        }

        close(fp);
        fclose(fd);//close rule file

    }

    return 0;
}
