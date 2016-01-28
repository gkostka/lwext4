#!/bin/sh

PREFIX_TO_ADD=LWEXT4
EXT4_ERRNO_H=ext4_errno.h
EXT4_OFLAGS_H=ext4_oflags.h
USED_ERRNO_FILE=/tmp/used_errno
USED_OFLAGS_FILE=/tmp/used_oflags

usage()
{
	printf "Usage: %s <lwext4_source_patch>" $1
	exit 1
}

if [[ $# -ne 1 ]];then
	usage $0
fi

# First change the working directory to destination.
cd $1
if [[ $? -ne 0 ]];then
	exit $?
fi

# Extract definitions from 2 files.
sed -n "s/[[:blank:]]*#define \(E[[:alnum:]]\+\)[[:blank:]]\+[[:alnum:]]\+[[:blank:]]*.*/\1/gp" ./include/$EXT4_ERRNO_H > $USED_ERRNO_FILE
sed -n "s/[[:blank:]]*#define \(O_[[:alpha:]]\+\)[[:blank:]]\+[[:alnum:]]\+[[:blank:]]*.*/\1/gp" ./include/$EXT4_OFLAGS_H > $USED_OFLAGS_FILE
sed -n "s/[[:blank:]]*#define \(SEEK_[[:alpha:]]\+\)[[:blank:]]\+[[:alnum:]]\+[[:blank:]]*.*/\1/gp" ./include/$EXT4_OFLAGS_H >> $USED_OFLAGS_FILE

# Add prefix to those definjtions.

for errno in $(cat $USED_ERRNO_FILE);do
	echo "For $errno"
	for file in $(find . -name "*.c" | xargs -n 1);do
		if [[ $(basename $file) != $EXT4_ERRNO_H ]];then
			sed -i "s/\\<${errno}\\>/${PREFIX_TO_ADD}_${errno}/g" $file
		fi
	done
	for file in $(find . -name "*.h" | xargs -n 1);do
		if [[ $(basename $file) != $EXT4_ERRNO_H ]];then
			sed -i "s/\\<${errno}\\>/${PREFIX_TO_ADD}_${errno}/g" $file
		fi
	done
done

for oflags in $(cat $USED_OFLAGS_FILE);do
	echo "For $oflags"
	for file in $(find . -name "*.c" | xargs -n 1);do
		if [[ $(dirname $file) != "./blockdev/"* &&  \
		      $(basename $file) != $EXT4_OFLAGS_H ]];then
			sed -i "s/\\<${oflags}\\>/${PREFIX_TO_ADD}_${oflags}/g" $file
		fi
	done
	for file in $(find . -name "*.h" | xargs -n 1);do
		if [[ $(dirname $file) != "./blockdev/"* &&  \
		      $(basename $file) != $EXT4_OFLAGS_H ]];then
			sed -i "s/\\<${oflags}\\>/${PREFIX_TO_ADD}_${oflags}/g" $file
		fi
	done
done

# Do final patching.
for patches in $(dirname $0)/*.patch ;do
	patch -p0 < $patches
done
