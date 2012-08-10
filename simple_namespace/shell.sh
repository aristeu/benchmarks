#!/bin/bash

if [ -z "$1" -o -z "$2" ]; then
	echo "$0 hostname cgroup_path" >&2;
	exit 1;
fi

unshare -m -u -i -n /stage2.sh "$1" "$2";
