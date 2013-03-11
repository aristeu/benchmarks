#!/bin/sh

# Source the common test script helpers
. /usr/bin/rhts_environment.sh

#    report_result $TEST $result

# ---------- Start Test -------------
# Setup some variables
# env params:   name                    default
#               NS_STRESS_TESTDIR       /mnt/testarea

if [ -z "$NS_STRESS_TESTDIR" ]; then
	NS_STRESS_TESTDIR=/mnt/testarea
fi
OUTPUTFILE=$NS_STRESS_TESTDIR/ns_stress.log

echo "Starting ns_stress" >$OUTPUTFILE;
./stress_namespaces >$OUTPUTFILE 2>&1;
if [ $? = 0 ]; then
	report_result "ns_stress" "PASS";
else
	report_result "ns_stress" "FAIL";
fi

exit 0
