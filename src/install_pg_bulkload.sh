#!/bin/sh
#
# Name  : install_pg_bulkload
# Author: Ambuj Singh
#
# Modification History:
#   30/01/2014  Initial version

CURRENT_PATH=`pwd`
# log file for pg_bulkload installation
CAPTION="/root/pg_bulkload.log"
touch $CAPTION
chmod 0666 $CAPTION

msg_out()
{
  echo "$1" >> $CAPTION
  echo "$1"
}

check_command_status()
{
  if [ $? -ne 0 ]; then
    msg_out "ERROR: $1 failed"
    msg_out "Exiting"
    exit 1
  fi
}

run_configure_and_make_install()
{
  ./configure >> $CAPTION 2>&1
  check_command_status "$1"
  make >> $CAPTION 2>&1
  check_command_status "$1"
  make install >> $CAPTION 2>&1
  check_command_status "$1"
}

make_pam_library()
{
  # extract dependency for libpam
  msg_out "extracting bison tar"
  msg_out "---------------------------------"
  tar -xvzf bison-3.0.tar.gz >> $CAPTION 2>&1
  check_command_status "untar bison"
  msg_out "extracting flex tar"
  msg_out "---------------------------------"
  tar -xvzf flex-2.5.37.tar.gz >> $CAPTION 2>&1
  check_command_status "untar flex"
 
  # make the libraries
  msg_out "making bison"
  cd $CURRENT_PATH/bison-3.0/ 
  run_configure_and_make_install "bison make"
  
  msg_out "making flex"
  cd $CURRENT_PATH/flex-2.5.37
  run_configure_and_make_install "flex make"

  # extract the libpam library
  cd $CURRENT_PATH

  msg_out "making pam"
  tar -xvzf pam_1.1.3.orig.tar.gz >> $CAPTION 2>&1
  check_command_status "untar pam"
  cd $CURRENT_PATH/Linux-PAM-1.1.3/
  run_configure_and_make_install "libpam make"
}

make_edit_library()
{
  msg_out "dpkg'ing libedit"
  dpkg -i libedit2_2.11-20080614-2.2_i386.deb >> $CAPTION 2>&1
  
  if [ -s /usr/lib/x86_64-linux-gnu/libedit.so.2 ]; then
    ln -s /usr/lib/x86_64-linux-gnu/libedit.so.2 /usr/lib/x86_64-linux-gnu/libedit.so
  else
    msg_out "libedit.so.2 not found. Exiting..."
    exit 1
  fi
}

make_pg_bulkload()
{
  msg_out "extracting the pg_bulkload tar"
  msg_out "---------------------------------"
  # untar the files
  tar -xvzf pg_bulkload-3.1.4.tar.gz >> $CAPTION 2>&1

  cd $CURRENT_PATH/pg_bulkload-3.1.4
  make USE_PGXS=1 >> $CAPTION 2>&1
  check_command_status "pg_bulkload make"
  make USE_PGXS=1 install >> $CAPTION 2>&1
  check_command_status "pg_bulkload make install"
}

cd $CURRENT_PATH
> $CAPTION

# untar the pg_bulkload tar to install the pg_bilkload
msg_out "Starting to install PG_BULKLOAD"
msg_out "====================================="
tar -xvzf ns_pg_bulkload.*.tar.gz >> $CAPTION 2>&1

CURRENT_PATH="$CURRENT_PATH/ns_pg_bulkload"

cd $CURRENT_PATH
msg_out "Making the pam library"
msg_out "====================================="
make_pam_library

cd $CURRENT_PATH
# check to seeif libedit.so is present
if [ -s /usr/lib/x86_64-linux-gnu/libedit.so ]; then
  #do nothing
  msg_out "libedit.so already present"
else
  if [ -s /usr/lib/x86_64-linux-gnu/libedit.so.2 ]; then
    # if the libedit.so.2 is already present, then make a link to libedit.so
    msg_out "making libedit.so link"
    ln -s /usr/lib/x86_64-linux-gnu/libedit.so.2 /usr/lib/x86_64-linux-gnu/libedit.so
  else
    # dpkg the package and make a link
    msg_out "Making the libedit library"
    msg_out "====================================="
    make_edit_library
    check_command_status "libedit make"
  fi
fi

cd $CURRENT_PATH
msg_out "Making the pg_bulkload"
msg_out "====================================="
make_pg_bulkload

# creating function for pg_bulkload
STATUS=`/etc/init.d/postgresql status | cut -d ':' -f 2`

msg_out "Checking postgresql status"
msg_out "====================================="
# if the psql is not running then start it 
# Currently we are supporting only for Ubuntu systems with Postgresql version $(POSTGRESQL_VERSION)
if [ "X$STATUS" != "X $POSTGRESQL_VERSION/main " ]; then
  msg_out "Starting postgresql..."
  /etc/init.d/postgresql start
  STATUS=`/etc/init.d/postgresql status | cut -d ':' -f 2`
  if [ "X$STATUS" != "X $POSTGRESQL_VERSION/main " ]; then
    msg_out "ERROR: Unable to start Postgresql..."
    exit 1
  else
    msg_out "POSTGRESQL is Started"
  fi
else
  msg_out "POSTGRESQL is Running"
fi

msg_out "Creating function pg_bulkload..."
msg_out "====================================="
cd $CURRENT_PATH/pg_bulkload-3.1.4/lib
ERROR_STR=`su postgres -c 'psql test postgres -f pg_bulkload.sql' 2>&1`
echo $ERROR_STR | grep "ERROR" 
if [ $? -ne 0 ]; then
  msg_out "pg_bulkload function created successfully..."
else
  msg_out "Could not create pg_bulkload function"
  echo "PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/games:/home/cavisson/work/bin:/home/cavisson/work/tools:$JAVA_HOME/bin" >>/etc/environment
  exit 1
fi

cd $CURRENT_PATH
cd ..

# delete all the files and the directory
rm -rf ns_pg_bulkload
rm -f ns_pg_bulkload.*.tar.gz

echo "PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/games:/home/cavisson/work/bin:/home/cavisson/work/tools:$JAVA_HOME/bin" >>/etc/environment

msg_out "Successfully installed pg_bulkload..."
# successfull return
exit 0
