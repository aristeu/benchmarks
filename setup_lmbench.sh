#!/bin/bash
function die
{
	echo "Error: $1";
	exit 1;
}

mkdir -p /opt && cd /opt || die "creating /opt";
rm -Rf /opt/lmbench3 /tmp/lmbench3;

(wget -o /dev/null -O - http://www.bitmover.com/lmbench/lmbench3.tar.gz | tar xvz ) || die "downloading lmbench";
cd lmbench3
(wget -o /dev/null -O - http://jake.ruivo.org/~aris/lmbench-buildfix.patch | patch -p1) || die "applying build fix";
make >/dev/null || die "building lmbench";
echo "CONFIGURING. you can cancel after the configuration is done"
make results;
