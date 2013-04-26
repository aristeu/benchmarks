#!/bin/bash

function die
{
	echo "Error: $1";
	exit 1;
}

git clone git://ltp.git.sourceforge.net/gitroot/ltp/ltp /tmp/ltp || die "unable to clone";
cd /tmp/ltp || die "directory not found" ;
make autotools >/dev/null || die "autotools";
./configure >/dev/null || die "configure";
make >/dev/null || die "build error";
make install || die "installation error";
rm -Rf /tmp/ltp

