#!/bin/bash
cgroup=/sys/fs/cgroup;
groups=blkio,net_cls;
tmp=$(mktemp -d);
file=blkio.io_service_time;

for dir in $(echo $groups | sed -e "s/,/\ /g"); do
	find $cgroup/$dir/ -depth -mindepth 1 -type d -exec rmdir {} \;
	umount $cgroup/$dir;
done

mount none -t cgroup -o $groups -o xattr $tmp;
chcon system_u:object_r:user_home_t $tmp/$file;
ls -Z $tmp/$file;
mount -o remount -o $(echo $groups | cut -f 1 -d ',') -o xattr $tmp;
ls -Z $tmp/$file;
mount -o remount -o $groups -o xattr $tmp;
ls -Z $tmp/$file;
umount $tmp;
rmdir $tmp;
