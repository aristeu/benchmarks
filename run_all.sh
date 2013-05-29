#!/bin/bash
OUTPUT=~/results/$(uname -r)/;
mkdir -p $OUTPUT;

function control_c
{
	true;
}

trap control_c SIGINT;

# ipcbench
echo "ipcbench";
cd /opt/ipcbench;
./run.py >/tmp/log;
file=$(cat /tmp/log | grep "Output written" | sed -e "s/.*Output written as //");
mv $file $OUTPUT/ipcbench.tar.gz

# lmbench3:
echo "lmbench3";
cd /opt/lmbench3
make rerun
mkdir -p $OUTPUT/lmbench3/;
cp /opt/lmbench3/results/x86_64-linux-gnu/* $OUTPUT/lmbench3;

# ltp
echo "ltp"
cd /opt/ltp
./runltplite.sh >$OUTPUT/ltp.log

# sysbench
echo "sysbench"
sysbench --test=oltp --mysql-table-engine=myisam --oltp-table-size=1000000 --max-requests=100000 --mysql-host=localhost --mysql-user=root run >$OUTPUT/sysbench.log

