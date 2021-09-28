#!/bin/bash

CAPTION="/root/install.sh.log"
touch $CAPTION
chmod 666 $CAPTION
chmod 755 /root
chmod 755 /root/cavisson_libs

echo "Starting Installation.. at `date`" >>$CAPTION
msgout ()
{
	echo "$1" >>$CAPTION
	echo "$1"
}

msgout "creating cavmodem device"
if [ -c "/dev/cavmodem" ];then
	msgout "cavmodem already  exist"
else
	mknod /dev/cavmodem c 10 209
	chmod 666 /dev/cavmodem
	msgout "cavmodem created"
fi

msgout "creating test_run_id db"
if [ -f "/etc/test_run_id" ];then
	msgout "test_run_id already  exist"
else
	echo 1000 > /etc/test_run_id
	chmod 666 /etc/test_run_id
	msgout "test_run_id created"
fi

#Add following lines in "/etc/sysctl.conf file
NUM=`grep -c Cavisson /etc/sysctl.conf`
if [ $NUM -eq 0 ];then
echo "#Added by Cavisson" >>/etc/sysctl.conf
echo "1048576" > /proc/sys/fs/file-max
echo "1024 65536" > /proc/sys/net/ipv4/ip_local_port_range
echo "100000" > /proc/sys/net/ipv4/tcp_max_syn_backlog
echo "fs.file-max = 1048576" >>/etc/sysctl.conf
echo "net.ipv4.tcp_max_syn_backlog=100000" >>/etc/sysctl.conf
echo "net.ipv4.ip_local_port_range = 1024 65536" >>/etc/sysctl.conf
fi

if [ ! -f /usr/include/linux/posix_types.h.orig ];then
cd /usr/include/linux
mv posix_types.h posix_types.h.orig
sed 's/#define __FD_SETSIZE	1024/#define __FD_SETSIZE	262144/g' posix_types.h.orig >posix_types.h
cd /root/cavisson_libs
fi

NUM=`grep -c Cavisson /etc/security/limits.conf`
if [ $NUM -eq 0 ];then
echo "#Added by Cavisson" >>/etc/security/limits.conf
echo "*               soft    nofile          262144" >>/etc/security/limits.conf
echo "*               hard    nofile          262144" >>/etc/security/limits.conf
echo "session    required     /lib/security/pam_limits.so" >>/etc/pam.d/login
fi

#create  user  cavisson  with password cavisson
msgout "create  user  cavisson  with password cavisson"
USRSTORM=`cat /etc/passwd|grep "^cavisson:"|wc -l`
if [ $USRSTORM -ne  1 ];then
	msgout "Creating user cavisson"
	useradd -p cavisson cavisson

	if [ $? -ne 0 ];then
		msgout "User Netstorm could not be created successfully"
		exit 1
	fi

fi

#change the directory  to "/root/cavisson_libs"
msgout "change the directory  to /root/cavisson_libs"
if [ ! -d /root/cavisson_libs ]
then
	msgout "/root/cavisson_libs does not exist"
	exit 1
fi

cd /root/cavisson_libs

#remove old zlib rpm's
msgout "Removing old zlibs"
for pkg in `rpm -qa | grep zlib`
do
rpm -e --nodeps $pkg
done

#install new zlibrpm
msgout "Intsalling zlib"
rpm -i zlib-1.2.2-2.i386.rpm
if [ $? -ne 0 ];then
	msgout "Unable to install zlib rpm"
	exit 1
fi
msgout "Intsalling zlib Devel"
rpm -i zlib-devel-1.2.2-2.i386.rpm
if [ $? -ne 0 ];then
	msgout "Unable to install zlib devel rpm"
	exit 1
fi

#untar the "postgresql-base-7.4.3.tar" file
msgout "untar the postgresql-base-7.4.3.tar file"
if [ ! -f /root/cavisson_libs/postgresql-base-7.4.3.tar.gz ]
then
	msgout "/root/cavisson_libs/postgresql-base-7.4.3.tar.gz not available"
	exit 1
fi

msgout "untar postgresql "
tar xvzf postgresql-base-7.4.3.tar.gz >>$CAPTION 2>&1

#check the directory to "postgresql-7.4.3"
msgout "check the directory to postgresql-7.4.3"
if [ ! -d /root/cavisson_libs/postgresql-7.4.3 ]
then
	msgout "untar unsuccessful. /root/cavisson_libs/postgresql directory was not created "
	exit 1
fi

cd postgresql-7.4.3

#Make sure you are in "/root/cavisson_libs/postgresql-7.4.3"
msgout "Make sure you are in  /root/cavisson_libs/postgresql-7.4.3"
WD=`pwd`
if [ $WD != "/root/cavisson_libs/postgresql-7.4.3" ]
then
	msgout "you should be in /root/cavisson_libs/postgresql-7.4.3 directory"
	exit 1;
fi

msgout "configure postgres..."
#To configure  package for your system
./configure >>$CAPTION 2>&1

#To  compile the package
if [ $? -ne 0 ]
then
	msgout "configure not successful.. exiting "
	exit 1
fi

msgout "making postgres..."
gmake >>$CAPTION 2>&1

if [ $? -ne 0 ]
then
	msgout "gmake not successful. Check at logs" 
	exit 1
fi

msgout "installing postgres..."
gmake install >>$CAPTION 2>&1
if [ $? -ne 0 ]
then
	msgout "postgress install not successful. Check at logs" 
	exit 1
fi

#To know the status of postgress
USRPOSTGRES=`cat /etc/passwd|grep "^postgres:"|wc -l`
if [ $USRPOSTGRES -ne  1 ]
then
	msgout "Creating the user postgres "
	useradd -p postgres postgres 
	#Add user postgres
	if [ $? -ne 0 ]; then
		msgout "User postgres could not be created." 
		exit 1
	fi
fi

#Make the directory "/usr/local/pgsql/data
msgout "Make the directory /usr/local/pgsql/data"
if [ ! -d /usr/local/pgsql/data ]
then
	mkdir /usr/local/pgsql/data
else
	/etc/init.d/postgresql stop
	rm -rf  /usr/local/pgsql/data
	mkdir /usr/local/pgsql/data
fi

#change the ownership  from root to postgres
msgout "change the ownership  from root to postgres"
chown postgres /usr/local/pgsql/data
if [ $? -ne 0 ];then
	msgout "Could not change ownership"
	exit 1
fi

#Change the user from "root" to "postgres"
msgout "Change the user from root to postgres"
su - postgres -c "/root/cavisson_libs/pginstall.sh"

if [ $? -ne 0 ];then
	msgout "postgres install unsuccessful"
	exit 1
fi
#Now root
#To overwrite the current one
msgout "copy postgresql... "
cp -f /var/lib/pgsql/postgresql /etc/init.d
if [ $? -ne 0 ]; then
	msgout "copy postgresql failed"
	exit 1
fi
chkconfig --del postgresql
chkconfig --add postgresql

##Installation of "Jakarta-Tomcat-5.5.4.tar.gz"
#To change the directory to "/apps"
mkdir -p /apps/java
if [ $? -ne 0 ]; then
	msgout "mkdir /apps/java failed"
	exit 1
fi
cd /apps

#To make sure it is correct directory  "/apps"
PW=`pwd`
if [ $PW != "/apps" ]
then
	msgout "unable to change directory to /apps "
	exit 1
fi

cp /root/cavisson_libs/jakarta-tomcat-5.0.14.tar.gz .

#Untar the file "jakarta-tomcat-5.0.14.tar.gz"
msgout "Untaring tomcat..."
tar xvzf  jakarta-tomcat-5.0.14.tar.gz >>$CAPTION 2>&1
if [ $? -ne 0 ]
then
	msgout "Untar tomcat is Unsuccessful..."
	exit 1
fi

#Make one directory  called "java" under "/apps"
#change the directory to "/apps/java"
cd  /apps/java

#To make sure it is right directory  "/apps/java"
PD=`pwd`
if [ $PD != "/apps/java" ]
then
	msgout "unable to cd /apps/java"
	exit 1
fi

#Copy "j2sdk-1_4_0-linux-i386.bin"  to current directory(/apps/java)
msgout "Getting jdk..."
cp /root/cavisson_libs/j2sdk-1_4_0-linux-i386.bin .
if [ $? -ne 0 ]; then
	msgout "unable to copy jdk"
	exit 1
fi

#Run "j2sdk-1_4_0-linux-i386.bin"
msgout "Installing jdk..."
./j2sdk-1_4_0-linux-i386.bin >>$CAPTION 2>&1  <<+
yes
+

if [ $? -ne 0 ] 
then
	msgout "jdk run is unsuccessful...."
	exit 1
fi


##Installation of "gperf-3.0.1.tar"
cd /root/cavisson_libs

#Untar  "gperf-3.0.1.tar" file
msgout "gperf Untar ...." 
tar xvfz gperf-3.0.1.tar.gz >>$CAPTION 2>&1
if [ $? -ne 0 ]
then
	msgout "gperf Untar is not successful...." 
	exit 1
fi

#change the directory to "gperf-3.0.1"
cd gperf-3.0.1

#To make sure it is correct directory  "/root/cavisson_libs/gperf-3.0.1"
WORKNG=`pwd`
if [ $WORKNG != "/root/cavisson_libs/gperf-3.0.1" ]
then
	msgout "Unable to cd to /root/cavisson_libs/gperf-3.0.1"
	exit 1
fi

#To configure the package for your system
msgout "configuration for gperf ..."
./configure >>$CAPTION 2>&1
if [ $? -ne 0 ]
then
	msgout "configuration for gperf is unsuccessful.."
	exit 1
fi

#To  compile the package
msgout "gperf Compilation..."
make >>$CAPTION 2>&1
if [ $? -ne 0 ]
then
	msgout "gperf Compilation is Unsuccessful..."
	exit 1
fi

#Optionally, to run any self test that come with the package
msgout "gperf make check..."
make check >>$CAPTION 2>&1
if [ $? -ne 0 ]
then
	msgout "gperf make check not done properly.."
	exit 1
fi

#to install the programs and any data files & documentation
msgout "gperf Installation.."
make install
if [ $? -ne 0 ]
then
	msgout "gperf Installation is Unsuccessful.."
	exit 1
fi


##Installation  of "gsl-1.1.1.tar"
#change the directory to "/root/cavisson_libs"
cd /root/cavisson_libs
if [ $? -ne 0 ]
then
	msgout "Unable to cd to /root/cavisson_libs "
	exit 1
fi

#Untar "gsl-1.1.1.tar" file
msgout "gsl  Untarring..."
tar xvfz gsl-1.1.1.tar.gz >>$CAPTION
if [ $? -ne 0 ]
then
	msgout "gsl  Untarring unsuccessful.."
	exit 1
fi

#change the directory to "gsl-1.1.1"
cd gsl-1.1.1

#To make sure it is right directory "/root/cavisson_libs/gsl-1.1.1"
WORK=`pwd`
if [ $WORK != "/root/cavisson_libs/gsl-1.1.1" ]
then
	msgout "could not cd to gsl-1.1.1 directory.."
	exit 1
fi

#To configure the package for your system
msgout "configure gsl-1.1.1..."
./configure >>$CAPTION 2>&1
if [ $? -ne 0 ]
then
	msgout "configure failed for gsl-1.1.1"
	exit 1
fi

#To compile the package
msgout "make gsl-1.1.1..."
make >>$CAPTION 2>&1
if [ $? -ne 0 ]
then
	msgout "make failed for gsl-1.1.1"
	exit 1
fi

#Optionally, to run any  self tests that come with package
msgout "make check gsl-1.1.1..."
make check >>$CAPTION 2>&1
if [ $? -ne 0 ]
then
	msgout "make check failed for gsl-1.1.1"
	exit 1
fi

#To install the programs and any data files & documentation
msgout "install gsl-1.1.1..."
make install >>$CAPTION 2>&1
if [ $? -ne 0 ]
then
	msgout "make install failed for gsl-1.1.1"
	exit 1
fi


##Installation of "openssl-0.9.7b.tar"
#change the directory to "/root/cavisson_libs"
cd /root/cavisson_libs

#Untar "openssl-0.9.7b.tar" file
msgout "openssl Untar..."  
tar xvfz openssl-0.9.7b.tar.gz >>$CAPTION 2>&1
if [ $? -ne 0 ]
then
	msgout "openssl Untar is not successful...."  
	exit 1
fi

#change the directory to  "openssl-0.9.7b"
cd openssl-0.9.7b

#To make sure it is correct directory "/root/cavisson_libs/openssl-0.9.7b"
WRK=`pwd`
if [ $WRK != "/root/cavisson_libs/openssl-0.9.7b" ]
then
	msgout "could not cd to openssl-0.9.7b directory.."
	exit 1
fi

#To configure the package for your system
msgout "openssl configure..."  
./config >>$CAPTION 2>&1
if [ $? -ne 0 ]
then
	msgout "config failed for openssl-0.9.7b"
	exit 1
fi

#To compile the package
msgout "openssl make..."  
make >>$CAPTION 2>&1
if [ $? -ne 0 ]
then
	msgout "make failed for openssl-0.9.7b"
	exit 1
fi

#After  successful build, the libraries should be tested. If it fails look at output
msgout "openssl make test..."  
make test >>$CAPTION 2>&1
if [ $? -ne 0 ]
then
	msgout "test failed for openssl-0.9.7b"
	exit 1
fi

#To install the programs and any data files & documentation
msgout "openssl make install..."  
make install >>$CAPTION 2>&1
if [ $? -ne 0 ]
then
	msgout "make install failed for openssl-0.9.7b"
	exit 1
fi


##Installation of "capture_src.tar.gz" file
#change the directory to "/apps"
#cd /apps

#cp /root/cavisson_libs/capture_src.tar.gz .
#Untar the file "capture_src.tar.gz"
#msgout "capture_src.tar.gz Untar ...." 
#tar xvzf capture_src.tar.gz >>$CAPTION 2>&1
#if [ $? -ne 0 ]
#then
#	msgout "capture_src.tar.gz Untar is not successful...." 
#	exit 1
#fi

#change the directory to "capture"
#cd capture

#To make sure it is right directory "/apps/capture"
#WK=`pwd`
#if [ $WK != "/apps/capture" ]
#then
#	msgout "unable to cd /apps/capture directory.."
#	exit 1
#fi
#
#change the ownership to cavisson for all files under "/apps/capture"
#chown -R cavisson:cavisson *

#change your permission from "root" to "cavisson"
msgout "Change the user from root to cavisson"
su - cavisson -c "/root/cavisson_libs/nsinstall.sh"

if [ $? -ne 0 ];then
	msgout "cavisson install unsuccessful"
	exit 1
fi

#/home/cavisson/work/bin/nsi_ns_upgrade

#As "root", Add following lines in "/etc/bashrc"
NUM=`grep -c "/apps/java/j2sdk1.4.0" /etc/bashrc`
if [ $NUM -eq 0 ];then
export JAVA_HOME=/apps/java/j2sdk1.4.0 >>/etc/bashrc
export PATH=$PATH:$JAVA_HOME/bin >>/etc/bashrc
fi
#cd /apps/jakarta-tomcat-5.0.14/conf
#cp server.xml server.xml.orig
#cp web.xml web.xml.orig
#cp /home/cavisson/work/webapps/.tomcat/*.xml .
#if [ $? -ne 0 ]
#then
#	msgout "copy tomcat config failed...." 
#	exit 1
#fi

#if [ ! -f /etc/init.d/tomcat ];then
#	cp /home/cavisson/work/webapps/.tomcat/tomcat /etc/init.d
#	chmod 777 /etc/init.d/tomcat
#	chkconfig --add tomcat
#	/etc/init.d/tomcat start
#fi

#if [ ! -f /etc/init.d/nsServer ];then
#	cp /home/cavisson/work/webapps/cavisson/realTimeGraph/server/nsServer /etc/init.d
#	chmod 777 /etc/init.d/nsServer
#	chkconfig --add nsServer
#	/etc/init.d/nsServer start
#fi
