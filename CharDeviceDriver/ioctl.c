#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "ioctl.h"

int main (int argc, char **argv) {
    
    char *filename; /* the name of the device */
    int fd; /* device file descriptor */
    int result;
	int nsize;
    
    if (argc != 3) {
	fprintf (stderr, "Useage: ./ioctl.h filename NewMaxSize\n");
	exit (1);
    }

    /* ioctl  can be performed only on opened device */
    filename = argv[1];
	nsize = atoi(argv[2]);
    fd = open (filename, O_RDONLY);
    if (fd == -1) {
	fprintf (stderr, "Could not open file %s, exiting!\n", filename);
	exit (1);
    }

    result = ioctl (fd, CHANGE_MAX_SIZE, nsize);
    printf ("The result of the ioctl is %d\n", result);
    
    close (fd);
    return 0;
}

    
    

	
