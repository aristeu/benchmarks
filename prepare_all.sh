#!/bin/bash

for i in setup_ipcbench.sh setup_ltp.sh setup_sysbench.sh setup_lmbench.sh; do
	bash ./$i || exit 1;
done
echo "All done";
