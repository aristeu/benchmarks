#!/bin/bash

if [ -z "$1" ]; then
	echo "$0 hostname [command]" >&2;
	exit 1;
fi

function get_free_inetaddr
{
	local x;
	local y;
	local tmp=$(mktemp);
	local found=0;

	ifconfig -a | grep "inet addr:" | sed -e "s/.*inet\ addr:\([0-9\.]\+\)\ .*/\1/" >$tmp;
	for x in $(seq 0 254); do
		for y in $(seq 0 254); do
			if [ -z "$(grep 10.x.y.1 $tmp)" ]; then
				found=1;
				break;
			fi
		done
		if [ $found = 1 ]; then
			break;
		fi
	done
	if [ $found = 1 ]; then
		echo "10.$x.$y";
		return 0;
	fi
	echo "Unable to get free addr" >&2;
	exit 1;
}

host=$1;
COMMAND="$2";
CONTAINER_ROOT=/lxc;
CGROUP_NAME=$host;
CGROUP_PATH=/sys/fs/cgroup/cpu,cpuacct/;
CGROUP=$CGROUP_PATH/$CGROUP_NAME;
IFACE1="$host"-system;
IFACE2="$host"-guest;
IP_BASE=$(get_free_inetaddr);
IP1=$IP_BASE.1;
IP2=$IP_BASE.2;


find $CGROUP/ -type d -delete;
ip link add name $IFACE1 type veth peer name $IFACE2;
ifconfig $IFACE1 $IP1 netmask 255.255.255.0 up;
ifconfig $IFACE2 $IP2 netmask 255.255.255.0 up;
echo 1 >/proc/sys/net/ipv4/ip_forward;
iptables -t nat -I POSTROUTING -s $IP2 -d 0.0.0.0/0 -j MASQUERADE;

mkdir -p $CGROUP;
$PWD/finish_networking.sh $CGROUP_PATH $CGROUP_NAME $IFACE2 &

chroot $CONTAINER_ROOT/$host /shell.sh $host $CGROUP_PATH/$CGROUP_NAME $IFACE2 $IP2 $IP1 $COMMAND;

