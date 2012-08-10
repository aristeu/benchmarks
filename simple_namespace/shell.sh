#!/bin/bash

if [ -z "$1" -o -z "$2" -o -z "$3" -o -z "$4" -o -z "$5" ]; then
	echo "$0 hostname cgroup_path iface ip router" >&2;
	exit 1;
fi

unshare -m -u -i -n /stage2.sh "$1" "$2" "$3" "$4" "$5";
