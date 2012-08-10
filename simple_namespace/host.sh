#!/bin/bash
MOCK="";
CONTAINER_ROOT=/lxc;

if [ -z "$1" ]; then
	echo "$0 <hostname>" >&2;
	exit 1;
fi

host="$1";

for i in bin dev etc home lib* media mnt opt proc root sbin srv tmp usr var; do
	mkdir -p $CONTAINER_ROOT/$host/$i;
	mount -o bind $MOCK/$i $CONTAINER_ROOT/$host/$i;
done

mkdir -p $CONTAINER_ROOT/$host/proc $CONTAINER_ROOT/$host/sys;
mount none -t proc $CONTAINER_ROOT/$host/proc
mount none -t sysfs $CONTAINER_ROOT/$host/sys

mount -o bind /sys/fs/cgroup $CONTAINER_ROOT/$host/sys/fs/cgroup

cp shell.sh stage2.sh $CONTAINER_ROOT/$host;
