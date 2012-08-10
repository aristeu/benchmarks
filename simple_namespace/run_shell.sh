#!/bin/bash
CGROUP_NAME=teste
CGROUP_PATH=/sys/fs/cgroup
CGROUP=$CGROUP_PATH/$CGROUP_NAME
IFACE1=veth0
IFACE2=veth1
IP1=10.0.0.1
IP2=10.0.0.2

rm -Rf $CGROUP;
ip link add name $IFACE1 type veth peer name $IFACE2;
ifconfig $IFACE1 $IP1 netmask 255.255.255.0 up;
ifconfig $IFACE2 $IP2 netmask 255.255.255.0 up;
echo 1 >/proc/sys/net/ipv4/ip_forward;
iptables -t nat -I POSTROUTING -s $IP2 -d 0.0.0.0/0 -j MASQUERADE;

$PWD/finish_networking.sh $CGROUP_PATH $CGROUP_NAME $IFACE2 &

chroot /lxc/host1 /root/shell.sh

