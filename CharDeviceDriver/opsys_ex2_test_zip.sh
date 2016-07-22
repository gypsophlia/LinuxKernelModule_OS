#!/bin/bash

#
# Operating Systems 2015
# University of Birmingham
#
# copyright: Michael Denzel <m.denzel@cs.bham.ac.uk>
#

### CONFIG ###
target=/tmp/
assignment_num=2
##############

# --- helper function ---
function opsys_testcase(){
	t="testcase 1"
	d=/dev/opsysmem
	executable=charDeviceDriver
	
	#cleanup
	rmmod $executable 2>/dev/null >/dev/null
	rm -f $d
	
	#load kernel driver
	echo "loading driver"
	insmod ./$executable.ko
	if [ $? -ne 0 ]
	then
		echo "ERROR: insmod failed"
		exit -1
	fi

	#mknod
	echo "mknod command"
	major=`dmesg | tail -n 1 | sed -n "s/\[[0-9. ]*\] 'mknod \/dev\/opsysmem c \([0-9]*\) 0'\./\1/p"`
	echo "major number: $major"
	mknod $d c $major 0
	if [ $? -ne 0 ]
	then
		echo "ERROR: mknod command failed (did you print \"'mknod /dev/opsysmem c <major> 0'.\"?)"
		rmmod $executable
		return -1
	fi
		
	#check file
	echo "checking file $d"
	line=`ls $d 2>/dev/null`
	if [ "$line" != "$d" ]
	then
		echo "ERROR: file $d does not exist after loading the kernel module"
		rmmod $executable
		return -1
	fi
	
	#write test
	echo "write test"
	echo "$t" > $d 2>/dev/null
	if [ $? -ne 0 ]
	then
		echo "ERROR: writing $d failed"
		rmmod $executable
		return -1
	fi

	#read test
	echo "read test"
	r=`head -n 1 < $d`
	if [ $? -ne 0 ]
	then
		echo "ERROR: reading $d failed"
		rmmod $executable
		return -1
	fi
	
	#check if same was read
	if [ "$r" != "$t" ]
	then
		echo "ERROR: $d: could not read what was written before"
		rmmod $executable
		return -1
	fi

	#unload module
	echo "unloading driver"
	rmmod $executable
	if [ $? -ne 0 ]
	then
		echo "ERROR: unloading kernel module failed"
		return -1
	fi
	
	return 0
}

# --- MAIN ---

#check
if [ $# -ne 1 ]
then
	echo "Syntax: $(basename $0) <path to zip file>"
	echo "zip file structure:"
	echo -e "\t<studentID>.zip\n\t└── assignment2\n\t    ├── Makefile\n\t    └── ..."
	exit -1
fi

#init
studentID=`echo "$1" | sed 's/\([0-9]\{7\}\).*/\1/'`
filetype=`file -b "$1" | awk '{print $1;}'`
cwd=`pwd`
zipfile=$(basename $1)

#check studentID
if [[ "$studentID" =~ ^[0-9]{7}$ ]]
then
	#check filetype
	if [ "$filetype" == "Zip" ]
	then
		#reset
		rm -rf $target/$studentID
		mkdir $target/$studentID

		#copy zip
		cp $1 $target/$studentID

		#unzip
		echo "unzipping"
		cd $target/$studentID
		unzip $zipfile >/dev/null 2>/dev/null
		if [ $? -ne 0 ]
		then
			echo "ERROR: unzip failed"
			exit -1
		fi

		#goto folder
		cd ./assignment$assignment_num
		if [ $? -ne 0 ]
		then
			echo "ERROR: zip file should include a folder assignment$assignment_num"
			exit -1
		fi

		#execute makefile
		echo "executing Makefile"
		make -f ./Makefile >/dev/null
		if [ $? -ne 0 ]
		then
			echo "ERROR: make failed"
			exit -1
		fi

		#basic test
		echo "basic test"
		export -f opsys_testcase
		su -c opsys_testcase
		if [ $? -eq 0 ]
		then
			echo "SUCCESS: unzip, make, basic test"
			echo "(please be aware that we will test more cases than the basic test)"
			exit 0
		else
			exit $?
		fi
	else
		echo "ERROR: Not a zip file"
		exit -1
	fi
else
	echo "ERROR: StudentID seems wrong"
	exit -1
fi

