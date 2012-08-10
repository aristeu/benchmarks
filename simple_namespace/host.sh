#!/bin/bash
MOCK="";
CONTAINER_ROOT=/lxc;

if [ -z "$1" ]; then
	echo "$0 <hostname>" >&2;
	exit 1;
fi

host="$1";

for i in bin dev etc home lib lib64 media mnt opt root sbin srv tmp usr var; do
	mkdir -p $CONTAINER_ROOT/$host/$i;
	mount -o bind $MOCK/$i $CONTAINER_ROOT/$host/$i;
done

mkdir -p $CONTAINER_ROOT/$host/proc $CONTAINER_ROOT/$host/sys;
mount none -t proc $CONTAINER_ROOT/$host/proc
mount none -t sysfs $CONTAINER_ROOT/$host/sys
mount none -t tmpfs $CONTAINER_ROOT/$host/sys/fs/cgroup
#for i in blkio cpu cpuacct cpu,cpuacct cpuset devices freezer memory net_cls perf_event; do
#	mkdir -p $CONTAINER_ROOT/$host/sys/fs/cgroup/$i;
#	mount none -t cgroup -o $i $CONTAINER_ROOT/$host/sys/fs/cgroup/$i;
#done

cp shell.sh stage2.sh $CONTAINER_ROOT/$host;
