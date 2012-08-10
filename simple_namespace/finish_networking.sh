#!/bin/bash

CGROUP_PATH=$1;
CGROUP_NAME=$2;
IFACE=$3;
CGROUP="$CGROUP_PATH/$CGROUP_NAME";

if [ -z "$CGROUP_PATH" -o -z "$CGROUP_NAME" -o -z "$IFACE" ]; then
	echo "$0 <cgroup mount path> <cgroup name> <iface>" >&2;
	exit 1;
fi

tries=5;
while [ 1 ]; do
	if [ -d $CGROUP ]; then
		if [ -n "$(cat $CGROUP/tasks)" ]; then
			d=0;
			for i in $(cat $CGROUP/tasks); do
				ip link set $IFACE netns $i;
				ifconfig $IFACE >/dev/null 2>/dev/null;
				if [ $? = 1 ]; then
					d=1;
					break;
				fi
			done
			if [ $d = 1 ]; then
				break;
			fi
			tries=$[tries - 1];
			if [ $tries = 0 ]; then
				echo "error waiting for new pid" >&2;
				break;
			fi
		fi
	fi
	sleep 1;
done

