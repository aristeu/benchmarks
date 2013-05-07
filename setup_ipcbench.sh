#!/bin/bash

function die
{
	echo "Error: $1";
	exit 1;
}

mkdir -p /opt;
rm -Rf /tmp/ipcbench /opt/ipcbench;
git clone http://github.com/avsm/ipc-bench.git /tmp/ipcbench || die "unable to clone";
yum install -y numactl-devel
mv /tmp/ipcbench /opt/
