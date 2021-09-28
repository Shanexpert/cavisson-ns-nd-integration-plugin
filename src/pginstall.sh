#!/bin/bash
export IDIR="$1"
export CAPTION="$2"

msgout ()
{
	echo "$1" >>$CAPTION
	echo "$1"
}

DISTRO=$($IDIR/nsi_get_linux_release_ex -d)
RELEASE=$($IDIR/nsi_get_linux_release_ex -r)

if [[ $RELEASE -eq 2004 || $DISTRO == "Redhat" ]];then
  POSTGRESQL_VERSION=12
else
  POSTGRESQL_VERSION=9.5
fi
export POSTGRESQL_VERSION

if [ $DISTRO == "Ubuntu" ]; then
  POSTGRES_PATH="/usr/lib/postgresql/$POSTGRESQL_VERSION/bin/"
  POSTGRES_INCLUDE_PATH=/usr/include/postgresql/$POSTGRESQL_VERSION/server
elif [ $DISTRO == "Redhat" ]; then
  POSTGRES_PATH="/usr/pgsql-12/bin/"
  POSTGRES_INCLUDE_PATH=/usr/pgsql-12/include
fi
PATH=$PATH:$POSTGRES_PATH

COUNT=`ps -lef|grep postgres|grep -v grep |wc -l`
if [ $COUNT -eq 0 ];then
  msgout "postgres is not running"
  exit 1
else
  msgout "postgres running"
fi

#To connect to database and create database
NUM=`psql -l | grep -w test | wc -l`
if [ $NUM -eq 1 ];then
  msgout "test db already exist"
else
  createdb test
  msgout "createdb out is $?"
  if [ `psql -l | grep test | wc -l` -ne 1 ];then
    msgout "Could not create test db "
    exit 1
  else
    msgout "create test db done"
  fi
fi

#To change the directory to "/var/lib/pgsql"
cd /var/lib/pgsql

rm -f dbfunc.tar.gz
cp $IDIR/dbfunc.tar.gz .
if [ $? -eq 1 ];then
	msgout "could not copy dbfunc.tar.gz "
	exit 1
fi

tar xvzf dbfunc.tar.gz >>$CAPTION 2>&1
if [ $? -ne 0 ];then
	msgout "untar of dbfunc.tar.gz unsuccessful" 
	exit 1
fi

msgout "percentile compilation ..."
#Compile percentile.c
gcc -D$DISTRO -DRELEASE=$RELEASE -I  $POSTGRES_INCLUDE_PATH -fpic -c percentile.c >>$CAPTION 2>&1
if [ $? -ne 0 ];then
   msgout "percentile compilation is not successful "
fi

#To make the library out of it
gcc -shared -o percentile.so percentile.o >>$CAPTION 2>&1
if [ $? -ne 0 ];then
  msgout "percetile.so build unsuccessful"
  exit 1
fi

psql test -f  insert_aggregate.sql >>$CAPTION 2>&1
if [ $? -ne 0 ]; then
	msgout "insert_aggregate.sql unsuccessful"
	exit 1
fi

psql test -f  schema.sql >>$CAPTION 2>&1
if [ $? -ne 0 ]; then
	msgout "schema.sql unsuccessful"
	exit 1
fi

echo "Going to create table for access control."
if psql -lqt | cut -d \| -f 1 | grep "access_control" ; then
  echo 'access_control is already present in db';
else
   createdb access_control
fi

exit 0
