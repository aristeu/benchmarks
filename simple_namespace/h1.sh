#!/bin/bash
MOCK=/var/lib/mock/fedora-17-x86_64/root/

mount -o bind $MOCK/bin /lxc/host1/bin
mount -o bind $MOCK/dev /lxc/host1/dev
mount -o bind $MOCK/etc /lxc/host1/etc
mount -o bind $MOCK/home /lxc/host1/home
mount -o bind $MOCK/lib /lxc/host1/lib
mount -o bind $MOCK/lib64 /lxc/host1/lib64
mount -o bind $MOCK/media /lxc/host1/media
mount -o bind $MOCK/mnt /lxc/host1/mnt
mount -o bind $MOCK/opt /lxc/host1/opt
mount -o bind $MOCK/proc /lxc/host1/proc
mount -o bind $MOCK/root /lxc/host1/root
mount -o bind $MOCK/sbin /lxc/host1/sbin
mount -o bind $MOCK/srv /lxc/host1/srv
mount -o bind $MOCK/tmp /lxc/host1/tmp
mount -o bind $MOCK/usr /lxc/host1/usr
mount -o bind $MOCK/var /lxc/host1/var
# ltp
mount -o bind /opt /lxc/host1/opt

mount none -t proc /lxc/host1/proc
mount none -t sysfs /lxc/host1/sys

mount -o bind /sys/fs/cgroup /lxc/host1/sys/fs/cgroup
