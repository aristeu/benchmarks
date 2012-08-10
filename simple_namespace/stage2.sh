#!/bin/bash
# now we're already in the right namespace

host=$1;
cgroup=$2;
echo $$ >$cgroup/tasks;

exec bash;
 
