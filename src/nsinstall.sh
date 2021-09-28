#!/bin/bash

export IDIR="$1"
export CAPTION="$2"
export NS_WDIR="$3"

SYS_FILES="ip_entries ip_bonding ip_properties site.env"

msgout ()
{
	echo "$1" >>$CAPTION
	echo "$1"
}

# Save files if  existing
function save_files()
{
  msgout "Saving Files - $SYS_FILES"
  for file in $SYS_FILES
  do
    if [ -f $NS_WDIR/sys/$file ]; then
      echo "Saving $NS_WDIR/sys/$file as /tmp/$file.$$" >> $CAPTION
      cp $NS_WDIR/sys/$file /tmp/$file.$$ 
    fi
  done
}

#Restore from Saved if  existing else create
function restore_files()
{
  msgout "Creating/Restoring Files - $SYS_FILES"
  mkdir -p /home/cavisson/work/sys
  chmod 777 /home/cavisson/work/sys
  chown cavisson /home/cavisson/work/sys
  for file in $SYS_FILES
  do
    if [ -f /tmp/$file.$$ ]; then
      echo "Restoring $NS_WDIR/sys/$file" >> $CAPTION
      mv /tmp/$file.$$ $NS_WDIR/sys/$file
    else
      echo "Creating $NS_WDIR/sys/$file" >> $CAPTION
      if [ $file == "ip_properties" ]; then
        echo 'USE_FIRST_IP_AS_GATEWAY 0' > $NS_WDIR/sys/$file
        echo 'DUT_LAYER 0' >> $NS_WDIR/sys/$file
        echo 'RESERVED_NETID 192.168.255.0' >> $NS_WDIR/sys/$file
        chmod 666 $NS_WDIR/sys/$file
      elif [ $file == "site.env" ]; then
        cp $IDIR/site.env $NS_WDIR/sys/$file
        chown cavisson $NS_WDIR/sys/$file
        chmod 777 $NS_WDIR/sys/$file
      else
        touch $NS_WDIR/sys/$file
        chown cavisson $NS_WDIR/sys/$file
        chmod 666 $NS_WDIR/sys/$file
      fi
    fi
  done
}

##Installation of "cavisson"
cd
#To make sure it is right directory "/home/cavisson"
NST=`pwd`
if [ $NST != "/home/cavisson" ]
then
  msgout "unable to change /home/cavisson directory.."
  exit 1
fi


save_files

#Make a directory "work" under "/home/cavisson"
rm -rf work
mkdir -p work/.rel

#change the directory to "work"
cd work

#Be sure you are  in "/home/cavisson/work"
NT=`pwd`
if [ $NT != "/home/cavisson/work" ]
then
	msgout "unable to change /home/cavisson/work directory.."
	exit 1
fi

#Get and untar the  "netstorm*.tar.gz" file
msgout "Copying upgrade shell and Netstorm build"

cp $IDIR/upgrade.sh .
cp $IDIR/netstorm*.tar.gz .

msgout "Installing Netstorm build using tar"
tar xvzf netstorm*.tar.gz >>$CAPTION 2>&1
if [ $? -ne 0 ]
then
	msgout "netstorm*.tar.gz Untar is not successful...." 
	exit 1
fi

restore_files

# Must be done after netstorm build is untared
export NS_WDIR=/home/cavisson/work
. ${NS_WDIR}/etc/netstorm.env

msgout "Calling nsi_ns_upgrade"
bin/nsi_ns_upgrade
#change the directory to webapps
cd webapps
if [ $? -ne 0 ]
then
	msgout "unable to cd /home/cavisson/work/webapps"
	exit 1
fi

#Get and untar gui software "gui*.tar.gz" file
msgout "Installing Netstorm GUI build using tar"
cp $IDIR/gui*.tar.gz .
tar xvzf gui*.tar.gz >>$CAPTION 2>&1
if [ $? -ne 0 ]
then
	msgout "gui*.tar.gz Untar is not successful...." 
	exit 1
fi

cp $IDIR/hpd*.tar.gz /home/cavisson/work/.rel
cp $IDIR/cmon*.tar.gz /home/cavisson/work/.rel

exit 0
