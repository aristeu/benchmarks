#!/bin/bash

CGROUPFS=/sys/fs/cgroup/devices;
test=$CGROUPFS/test;

function die
{
	echo "$1: $2" >&2;
	exit 1;
}

# $1: test description
# $2: expected behavior
# $3: directory
function expect_behavior
{
	if [ ! "$(cat $3/devices.behavior)" = "$2" ]; then
		die "$1" "$3 doesn't have expected behavior (deny)";
	fi
}

# $1: test description
# $2: directory
function expect_empty
{
	if [ -n "$(cat $2/devices.exceptions)" ]; then
		die "$1" "$2 exceptions list isn't empty";
	fi
}

# $1: test description
# $2: directory
# $3: rule
function expect_rule
{
	if [ -z "$(grep "^$3\$" $2/devices.exceptions)" ]; then
		die "$1" "$2 exception rule not found: $3";
	fi
}

# $1: directory
function init
{
	local test=$1;

	mkdir -p $test/{a,b}
	mkdir -p $test/a/1
	for i in $test $test/b; do
		echo 'a' >$i/devices.allow;
	done
	for i in $test/a $test/a/1; do
		echo 'a' >$i/devices.deny;
	done
}

# $1: directory
function cleanup
{
	local test=$1;
	find $test -type d -delete;
}

# checking behavior propagation:
#    allow
#   /     \
# deny   allow
#  |
# deny

init $test;

echo 'a' >$test/devices.deny;
for i in $test $test/{a,b} $test/a/1; do
	expect_behavior "test1.1" "deny" "$i";
done

echo 'a' >$test/devices.allow;
for i in $test $test/b; do
	expect_behavior "test1.2" "allow" "$i";
done
for i in $test/a $test/a/1; do
	expect_behavior "test1.3" "deny" "$i";
done

cleanup $test;

# checking exception propagation:
#
# 1) simple propagation
#    - rule blocks a certain device
#    - no conflicting rules exist
#
#    allow
#   /     \
# deny   allow
#  |
# deny

init $test;

echo "b 12:34 rwm" >$test/a/devices.allow
echo "b 12:34 rwm" >$test/a/1/devices.allow
echo "b 12:35 rwm" >$test/a/devices.allow

expect_rule "test 2.1" "$test/a" "b 12:34 rwm";
expect_rule "test 2.2" "$test/a/1" "b 12:34 rwm";
expect_rule "test 2.3" "$test/a" "b 12:35 rwm";

echo "a" >$test/devices.deny
for i in $test $test/{a{,/1},b}; do
	expect_empty "test 2.4" "$i";
done

echo "a" >$test/devices.allow;

expect_rule "test 2.5" "$test/a" "b 12:34 rwm";
expect_rule "test 2.6" "$test/a/1" "b 12:34 rwm";
expect_rule "test 2.7" "$test/a" "b 12:35 rwm";

echo "b 12:35 rwm" >$test/a/devices.deny;

cleanup $test;

# exception propagation
# - deny1 allows device X
# - deny2 allows device X by inheritance
# - deny2 explicitely tries to allow X
# - deny1 gets changed to not allow X anymore
# - check if deny2's exception is gone
# - deny1 gets X allowed again
# - check if deny2's exception is still gone

init $test;

echo "b 12:34 rwm" >$test/a/devices.allow;
expect_rule "test 3.1" "$test/a" "b 12:34 rwm";
expect_empty "test 3.2" "$test/a/1";
echo "b 12:34 rwm" >$test/a/1/devices.allow;
expect_rule "test 3.2" "$test/a/1" "b 12:34 rwm";

echo "b 12:34 rwm" >$test/a/devices.deny;
for i in $test/a $test/a/1; do
	expect_empty "test 3.3" "$i";
done

cleanup $test;

# exception propagation 2
# - deny1 allows device X by mask
# - deny2 allows device X explicitely
# - deny1 gets the mask removed
# - check if deny2's exception is gone
# - deny1 gets the mask back in
# - check if deny2's exception is back

init $test;

echo "b 12:* rwm" >$test/a/devices.allow;
expect_rule "test 4.1" "$test/a" "b 12:\* rwm";
echo "b 12:34 rwm" >$test/a/1/devices.allow;
expect_rule "test 4.2" "$test/a/1" "b 12:34 rwm";

echo "b 12:* rwm" >$test/a/devices.deny;
for i in $test/a $test/a/1; do
	expect_empty "test 4.3" "$i";
done

cleanup $test;
