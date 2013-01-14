#!/bin/bash

CGROUPFS=/sys/fs/cgroup;
findargs="-maxdepth 5 -mindepth 1";
subsys_list="devices";
START_FILE=/tmp/go;
LABELLOOP_COUNT=400
CGROUPLOOP_COUNT=100

rm -f $START_FILE;
dir=$(mktemp -d);

if [ ! -d ${CGROUPFS} ]; then
	if osver=$(rpm -q --qf '%{version}\n' --whatprovides redhat-release); then
		if [[ $osver < 6 ]]; then
			echo "cgroups isn't supported on rhel$osver"
			exit 1
		else
			CGROUPFS="/cgroup"
			if ! rpm -q libcgroup 2>&1 > /dev/null; then
				yum -y install libcgroup
			fi
			# libcgroup should be installed by now
			if ! rpm -q libcgroup 2>&1 > /dev/null; then
				echo "libcgroup needed but can't be found/installed"
				exit 1
			fi
			# cgconfig need to be running
			cgstatus=$(service cgconfig status)
			if [[ x"$cgstatus" != "xRunning" ]]; then
				if ! service cgconfig start; then
					echo "Problem starting cgconfig"
					exit 1
				fi
			fi
		fi
				
	fi
fi

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

function test_devices
{
	for dir in $(find $CGROUPFS/devices/ -type d); do
		if [ $[$RANDOM % 2] = 0 ]; then
			rand="allow";
		else
			rand="deny";
		fi
		case $[$RANDOM % 4] in
			0)
			echo "c 12:34 r" >$dir/devices.$rand;
			break;
			;;
			1)
			echo "c 12:* rwm" >$dir/devices.$rand;
			break;
			;;
			2)
			echo "c *:34 rwm" >$dir/devices.$rand;
			break;
			;;
			3)
			echo "c *:* rwm" >$dir/devices.$rand;
			break;
			;;
		esac
	done
	# always keep the root allowing all
	echo a >$CGROUPFS/devices/devices.allow;
}

for i in $(seq 1 $CGROUPLOOP_COUNT); do
	create_cgroup 2>/dev/null &
	remove_cgroup 2>/dev/null &
	test_devices 2>/dev/null &
	true;
	echo -n ".";
done
for i in $(seq 1 $LABELLOOP_COUNT); do
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

