#!/bin/bash
VERSION="0.4.12";
URL="http://downloads.sourceforge.net/project/sysbench/sysbench/$VERSION/sysbench-$VERSION.tar.gz?r=http%3A%2F%2Fsourceforge.net%2Fprojects%2Fsysbench%2F&ts=1364915698&use_mirror=iweb"
function die
{
	echo "Error: $1";
	exit 1;
}

yum install -y mysql-server mysql-devel
cd /tmp; wget -O systench-$VERSION.tar.gz $URL || die "wget"; 
tar xfz sysbench-$VERSION.tar.gz || die "tar";
cd sysbench-$VERSION/ || die "directory not found" ;
(./configure && echo=/bin/echo make install) || die "build";
rm -Rf /tmp/systench-$VERSION/
service mysql start;
echo "create database sbtest;" | mysql -u root;
sysbench --test=oltp --mysql-table-engine=myisam --oltp-table-size=1000000 --mysql-host=localhost --mysql-user=root prepare
