#!/bin/bash

function die
{
	echo "Error: $1";
	exit 1;
}

git clone http://github.com/avsm/ipc-bench.git /tmp/ipcbench || die "unable to clone";
yum install -y numactl-devel
mv /tmp/ipcbench /opt/
