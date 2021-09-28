#!/bin/sh

export IDIR=`pwd`
CAPTION="/root/cav_install.log"

msgout ()
{
	echo "$1" >>$CAPTION
	echo "$1"
}

USERID=`id -u`
if [ $USERID -ne 0 ];then
    msgout "You must be root to execute this command"
    exit -1
fi

echo "Starting NetStorm Post Installation.. at `date`" >>$CAPTION

# Source cavisson.env to set environemnt variables which are based on JDK and Tomcat version
# Also NS_WDIR and PATH is set
export NS_WDIR=/home/cavisson/work
. $IDIR/netstorm.env

CONFIG=`cat $HOME_DIR/etc/cav.conf | grep CONFIG | awk '{print $2}'`
NSAdminIP=`cat $HOME_DIR/etc/cav.conf | grep NSAdminIP | awk '{print $2}' | awk -F / '{print $1}'`
SRAdminIP=`cat $HOME_DIR/etc/cav.conf | grep SRAdminIP | awk '{print $2}' | awk -F / '{print $1}'`

msgout "Creating files for password-less ssh"

mkdir -p /root/.ssh
chmod 700 /root/.ssh
#Delete only selected files
rm -f /root/.ssh/id_rsa /root/.ssh/id_rsa.pub

mkdir -p /home/cavisson/.ssh
chmod 700 /home/cavisson/.ssh
chown cavisson /home/cavisson/.ssh
chgrp cavisson /home/cavisson/.ssh
rm -f /home/cavisson/.ssh/id_rsa /home/cavisson/.ssh/id_rsa.pub

msgout "Create Keys for root id"
ssh-keygen -t rsa -P "" -f /root/.ssh/id_rsa >> $CAPTION 2>&1

msgout "Create Keys for cavisson id"
su cavisson -c 'ssh-keygen -t rsa -P "" -f /home/cavisson/.ssh/id_rsa' >> $CAPTION 2>&1

msgout "Copying ssh keys files to other machine for password-less ssh"
if [ $CONFIG == 'NS>NO' ];then
    ping -c 1 $SRAdminIP >/dev/null 2>&1
    if [ $? -ne 0 ];then
        msgout "Unable to connect to Server Admnin IP $SRAdminIP"
        msgout "Please make sure server is up before running post_install.sh"
        exit 1
    fi

    msgout "Copying /root/.ssh/id_rsa.pub to netocean machine as /root/.ssh/authorized_keys2"
    msgout ""
    msgout "You may be prompted for confirmation and root id password of netocean machine  - ${SRAdminIP}"
    msgout ""
    scp /root/.ssh/id_rsa.pub root@${SRAdminIP}:/root/.ssh/authorized_keys2
    msgout "Copying /home/cavisson/.ssh/id_rsa.pub to netocean machine as /home/cavisson/.ssh/authorized_keys2"
    msgout ""
    msgout "You may be prompted for confirmation and cavisson id password of netocean machine  - ${SRAdminIP}"
    msgout ""
    scp /home/cavisson/.ssh/id_rsa.pub cavisson@${SRAdminIP}:/home/cavisson/.ssh/authorized_keys2

    #Must run ssg once so that it can ask for confirmation
    msgout "Checking password-less ssh using root id by running pwd command"
    ssh ${SRAdminIP} pwd
    msgout "Checking password-less ssh using cavisson id by running pwd command"
    su  cavisson -c "ssh ${SRAdminIP} pwd"

elif [ $CONFIG == 'NS+NO' ];then
    msgout "Copying /root/.ssh/id_rsa.pub to /root/.ssh/authorized_keys2"
    cp /root/.ssh/id_rsa.pub /root/.ssh/authorized_keys2
    msgout "Copying /home/cavisson/.ssh/id_rsa.pub to /home/cavisson/.ssh/authorized_keys2"
    cp /home/cavisson/.ssh/id_rsa.pub /home/cavisson/.ssh/authorized_keys2
elif [ $CONFIG == 'NS' -o $CONFIG == 'ED' ];then
    msgout "No need for password-less ssh for NS/ED configuration"
elif [ $CONFIG == 'NV' ];then
    msgout "No need for password-less ssh for NV configuration"
elif [ $CONFIG == 'NCH' ];then
    msgout "No need for password-less ssh for NCH configuration"
elif [ $CONFIG == 'NO' ];then
    ping -c 1 $NSAdminIP >/dev/null 2>&1
    if [ $? -ne 0 ];then
        msgout "Unable to connect to Netstorm Admnin IP $NSAdminIP"
        msgout "Please make sure Netstorm is up before running post_install.sh"
        exit 1
    fi
    msgout "Copying /root/.ssh/id_rsa.pub to cavisson machine as /root/.ssh/authorized_keys2"
    msgout ""
    msgout "You may be prompted for confirmation and root id password of cavisson machine  - ${NSAdminIP}"
    msgout ""
    scp /root/.ssh/id_rsa.pub root@${NSAdminIP}:/root/.ssh/authorized_keys2
    msgout "Copying /home/cavisson/.ssh/id_rsa.pub to cavisson machine as /home/cavisson/.ssh/authorized_keys2"
    msgout ""
    msgout "You may be prompted for confirmation and cavisson id password of cavisson machine  - ${NSAdminIP}"
    msgout ""
    scp /home/cavisson/.ssh/id_rsa.pub cavisson@${NSAdminIP}:/home/cavisson/.ssh/authorized_keys2

    #Must run ssg once so that it can ask for confirmation
    msgout "Checking password-less ssh using root id by running pwd command"
    ssh ${NSAdminIP} pwd
    msgout "Checking password-less ssh using cavisson id by running pwd command"
    su  cavisson -c "ssh ${NSAdminIP} pwd"
else
    echo "Invalid Cavisson Config: $CONFIG. Valid values are NS, NO, NCH, NS+NO or NS>NO"
    exit 1
fi


msgout "Checking configuration integrity"
$NS_WDIR/bin/nsu_check_config

#Disable X windows

cp /etc/inittab //tmp/inittab.save
#sed "/x:5:once:\/etc\/X11\/prefdm -nodaemon/c\#x:5:once:/etc/X11/prefdm -nodaemon" /tmp/inittab.save > /etc/inittab
sed 's/^id:.:initdefault:$/id:3:initdefault:/' /tmp/inittab.save > /etc/inittab

msgout "NetStorm post installation Complete"
exit 0

