#!/bin/bash
URL=https://github.com/linux-test-project/ltp;

function die
{
	echo "Error: $1";
	exit 1;
}

mkdir -p /opt;
rm -Rf /tmp/ltp /opt/ltp;
git clone $URL /tmp/ltp || die "unable to clone";
cd /tmp/ltp || die "directory not found" ;
make autotools >/dev/null || die "autotools";
./configure >/dev/null || die "configure";
make >/dev/null || die "build error";
make install || die "installation error";
rm -Rf /tmp/ltp

