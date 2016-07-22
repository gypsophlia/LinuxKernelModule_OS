#!/bin/bash

#
# Operating Systems 2015
# University of Birmingham
#
# copyright: Michael Denzel <m.denzel@cs.bham.ac.uk>
#

### CONFIG ###
target=/tmp/
assignment_num=4
##############

# --- helper function ---
function opsys_testcase(){
    #in $target folder of student solution (e.g. /tmp/1234567/assignment2/)
    #makefile already executed
    kmodule=minix.ko
    procfile=/proc/minix
    
    #checks
    if [ $# -ne 1 ]; then
	echo "ABORT: missing studentID and target"
        return -1
    fi
    target=$1
    
    ###########################
    ##### TESTS TO REPLACE ####
    ###########################

    #minix file system file
    minixfs=${target}/mxtest
    mountpoint=/mnt/mxtest

    # === init ===
    #insert kernel module (this should be done before mounting the minix file system!)
    echo -en "insmod:\t\t"
    rmmod $kmodule 2>/dev/null >/dev/null #reset
    insmod $kmodule
    if [ $? -ne 0 ]; then
        echo "ERROR: insmod failed"
        return -1
    else
	echo "ok"
    fi
    
    #create minix file system of size 100 KB
    echo -en "create minix:\t"
    dd if=/dev/zero of=$minixfs bs=1024 count=100 >/dev/null 2>/dev/null
    if [ $? -ne 0 ]; then
        echo "ABORT: dd failed"
        return -1
    fi
    #create file system on file
    mkfs.minix $minixfs >/dev/null
    if [ $? -ne 0 ]; then
        echo "ABORT: mkfs failed"
        return -1
    fi
    #create mount point
    mkdir -p $mountpoint >/dev/null
    if [ $? -ne 0 ]; then
        echo "ABORT: mkdir $mountpoint failed"
        return -1
    fi
    #mount minix file system
    mount -o loop $minixfs $mountpoint >/dev/null
    if [ $? -ne 0 ]; then
        echo "ABORT: mount $minixfs failed"
        return -1
    fi
    echo "ok"

    # === tests ===
    testfile=$mountpoint/testfile
    #reset
    rm -f $testfile
    #create file
    touch $testfile
    if [ $? -ne 0 ]; then
        echo "ABORT: touch $testfile failed"
        return -1
    fi

    #get user id and ino
    uid=`id -u user`
    if [ "$uid" == "" ]; then
        echo "ABORT: empty uid"
        return -1
    fi
    ino=`ls -i $testfile | awk '{print $1;}'`
    if [ "$ino" == "" ]; then
        echo "ABORT: empty ino"
        return -1
    fi
    echo "(setting uid $uid ino $ino for file $testfile)"

    #change ino (check here for the input format)
    echo -en "write proc:\t"
    echo "$uid $ino $testfile" > $procfile
    if [ $? -ne 0 ]; then
        echo "ERROR: chaning $testfile (ino $ino) to uid $uid failed"
        return -1
    else
	echo "ok"
    fi

    #TODO: add your own tests here (e.g. fsck.minix -f or
    # checking the permissions of $testfile ;-) )
    # minix filesystem is mounted at /mnt/mxtest
    # minix file is at /tmp/mxtest

    #unmount
    echo -en "unmount:\t"
    umount $mountpoint
    if [ $? -ne 0 ]; then
        echo "ERROR: unmount failed"
        return -1
    else
	echo "ok"
    fi
    
    #remove kernel module
    echo -en "rmmod:\t\t"
    rmmod $kmodule
    if [ $? -ne 0 ]; then
        echo "ERROR: rmmod failed"
        return -1
    else
	echo "ok"
    fi
    
    return 0
}

# --- MAIN ---

#check
if [ $# -ne 1 ]
then
    echo "Syntax: $(basename $0) <path to zip/tar file>"
    echo "zip file structure:"
    echo -e "\t<studentID>.zip or <studentID>.tar or <studentID>.tar.gz\n\t└── assignment${assignment_num}\n\t    ├── Makefile\n\t    └── ..."
    exit -1
fi

#init
studentID=`echo "$1" | sed 's/\([0-9]\{7\}\).*/\1/'`
filetype=`file -b "$1" | awk '{print $1;}'`
cwd=`pwd`
archive=$(basename $1)

#check studentID
if [[ "$studentID" =~ ^[0-9]{7}$ ]]
then
    #reset
    rm -rf $target/$studentID
    mkdir $target/$studentID

    #copy zip
    cp $1 $target/$studentID

    #uncompress archive
    cd $target/$studentID
    case $filetype in
	#unzip
	Zip) unzip "$archive" >/dev/null 2>/dev/null
	     if [ $? -eq 0 ]; then
		 #everything fine, delete archive
		 rm "$archive"
	     else
		 echo "ERROR: unzip for student $studentID failed: $?"
		 exit -1
	     fi
	     ;;
	#un-tar
	POSIX|gzip) tar -xf "$archive" 1>&2 2>/dev/null
		    if [ $? -eq 0 ]
		    then
			#everything fine, delete archive
			rm "$archive"
		    else
			echo "ERROR: tar -x for student $studentID failed: $?"
			exit -1
		    fi
		    ;;
	*) echo "invalid compression: student $studentID filetype $filetype" ;;
    esac

    #goto folder
    cd ./assignment$assignment_num
    if [ $? -ne 0 ]
    then
	echo "ERROR: zip file should include a folder assignment$assignment_num"
	exit -1
    fi

    #execute makefile
    echo -en "exec Makefile:\t"
    make -f ./Makefile -B >/dev/null
    if [ $? -ne 0 ]
    then
	echo "ERROR: make failed"
	exit -1
    else
	echo "ok"
    fi
    echo ""

    #basic test
    export -f opsys_testcase
    su -c "opsys_testcase $target"
    if [ $? -eq 0 ]
    then
        echo -e "basic test:\tsuccess"
	exit 0
    else
        echo "ERROR: testcase failed"
	exit $?
    fi
else
    echo "ERROR: StudentID seems wrong"
    exit -1
fi

