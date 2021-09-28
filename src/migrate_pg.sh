#!/bin/sh

export IDIR=`pwd`
CAPTION="/root/cav_install.log"

migrate_ns_database()
{
  echo "Creating postgress test database for netstorm"
  /etc/init.d/postgresql stop >/dev/null 2>&1

  #Make the directory "/usr/local/pgsql/data
  echo "Making dir /usr/local/pgsql"
  mkdir -p /home/cavisson/pgsql

  echo "Moving pg data"
  cp -r /var/lib/pgsql/data /home/cavisson/pgsql

  echo "Making bakup of pg data"
  mv /var/lib/pgsql/data /var/lib/pgsql/data_bk

  #change the ownership  from root to postgres
  echo "Changing the ownership of postgress test database from root to postgres"
  chown -R postgres:postgres /home/cavisson/pgsql
  if [ $? -ne 0 ];then
    echo "Could not change ownership"
    exit 1
  fi

  #making the link /var/lib/pgsql/data to /home/cavisson/pgsql/data
  ln -s /home/cavisson/pgsql/data /var/lib/pgsql/data  
  if [ $? -ne 0 ];then
    echo "Could not link /var/lib/pgsql/data to /home/cavisson/pgsql/data"
    exit 1
  fi

  #forcely reload the pg data
  echo "Reloading the pg data"
  /etc/init.d/postgresql force-reload >/dev/null 2>&1

  STATUS=`/etc/init.d/postgresql status | grep "is running"`
  if [ "X$STATUS" != "X" ];then
    echo "Removing back up" 
    #rm -rf /var/lib/pgsql/data_bk
    echo "pg data migrated succefully"
  fi  
}

migrate_ns_database

exit 0
