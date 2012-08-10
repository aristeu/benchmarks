#!/bin/bash
# now we're already in the right namespace

host=$1;
cgroup=$2;
iface=$3;
ip=$4;
router=$5;

command=bash;
if [ -n "$6" ]; then
	command="$6";
fi

echo $$ >$cgroup/tasks;

hostname $host;

for i in $(seq 1 10); do
	if [ -n "$(ifconfig -a | grep $iface)" ]; then
		ifconfig $iface $ip netmask 255.255.255.0 up;
		route add default gw $router;
		break;
	fi
	echo "Waiting for the network interface";
	sleep 1;
done

exec $command;
 
