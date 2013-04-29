#!/bin/bash
for i in setup_ipcbench.sh setup_lmbench.sh setup_ltp.sh setup_sysbench.sh; do
	./$i || exit 1;
done
echo "All done";
