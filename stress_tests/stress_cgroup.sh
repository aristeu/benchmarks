#!/bin/bash

CGROUPFS=/sys/fs/cgroup;
findargs="-maxdepth 5 -mindepth 1";
subsys_list="blkio cpu cpuacct cpu,cpuacct cpuset devices freezer memory net_cls";
START_FILE=/tmp/go;

rm -f $START_FILE;
dir=$(mktemp -d);

function wait_start
{
	while [ 1 ]; do
		if [ -f $START_FILE ]; then
			break;
		fi
		sleep 1;
	done
}

function start
{
	touch $START_FILE;
}

function started
{
	mktemp -p $dir;
}

function finished
{
	rm -f $1;
}

function create_cgroup
{
	local s=$(started);
	wait_start;

	for c in $subsys_list; do
		if [ $[$RANDOM % 2 ] = 1 ]; then
			continue;
		fi
		for dir in $(find $CGROUPFS/$c/ $findargs -type d); do
			if [ $[$RANDOM % 3] = 0 ]; then
				mktemp -d -p $dir cgrouptest.XXXXXXXXXX >/dev/null 2>&1;
			fi
		done
	done
	finished $s;
}

function remove_cgroup
{
	local s=$(started);
	wait_start;

	for c in $subsys_list; do
		if [ $[$RANDOM % 2 ] = 1 ]; then
			continue;
		fi
		for dir in $(find $CGROUPFS/$c/ $findargs -type d); do
			if [ $[$RANDOM % 3] = 0 ]; then
				rmdir $dir >/dev/null 2>&1;
			fi
		done
	done
	finished $s;
}

function change_label
{
	local s=$(started);
	wait_start;

	for c in $subsys_list; do
		if [ $[$RANDOM % 2 ] = 1 ]; then
			continue;
		fi
		for file in $(find $CGROUPFS/$c/ -name tasks); do
			case $[$RANDOM % 3] in
				0)
				chcon "system_u:object_r:user_home_t" $file;
				break;
				;;
				1)
				chcon "system_u:object_r:httpd_sys_content_t" $file;
				break;
				;;
				2)
				setfattr -x security.selinux $file;
				break;
				;;
			esac
		done
	done
	finished $s;
}

for i in $(seq 1 50); do
	create_cgroup 2>/dev/null &
	remove_cgroup 2>/dev/null &
	true;
	echo -n ".";
done
for i in $(seq 1 200); do
	change_label 2>/dev/null &
	true;
	echo -n ".";
done
echo;

echo "All created, starting test";
start;

sleep 5;
while [ 1 ]; do
	if [ $(ls $dir | wc -l) = 0 ]; then
		break;
	fi
	sleep 1;
done
rmdir $dir;
rm -f $START_FILE;

