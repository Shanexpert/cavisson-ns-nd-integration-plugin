#! /bin/sh

isNetstromRunning()		# This function is to keep tracking weather test run(s) are ruuning while upgrading. 
{
  nFlag=`$NS_WDIR/bin/nsu_show_netstorm`
  if [ $? -eq 0 ];then
    echo "One or more test runs are running at this time. Please stop these test runs before doing upgrade."
    echo "Following test run(s) are running at this time : "
    echo "$nFlag"
    exit 1
  fi
}


#----Add By Atul-----

isNetstromRunning

if [ -f $NS_WDIR/bin/nsu_delete_url_based_script ];then
  $NS_WDIR/bin/nsu_delete_url_based_script -f
fi

#----End-------------


if [ "$#" -eq 1 ];then
	FILE=$1
	if [ ! -f $FILE ];then
	    echo "Input file ($FILE) is does not exist"
	    exit 1
	fi
fi

if [ "X$TOMCAT_DIR" = "X" ];then
	echo "TOMCAT_DIR must be defined"
	exit 1
fi
if [ "X$NS_WDIR" = "X" ];then
	echo "NS_WDIR must be defined"
	exit 1
fi
if [ "X$TOMCAT_CMD" = "X" ];then
	echo "TOMCAT_CMD must be defined"
	exit 1
fi

if [ "X$FHOST" = "X" ];then
	echo "FHOST must be defined"
	exit 1
fi

if [ "X$FUSER" = "X" ];then
	echo "FUSER must be defined"
	exit 1
fi

if [ "X$FPASS" = "X" ];then
	echo "FPASS must be defined"
	exit 1
fi

if [ "X$FNDIR" = "X" ];then
	echo "FNDIR must be defined"
	exit 1
fi

if [ "X$FGDIR" = "X" ];then
	echo "FGDIR must be defined"
	exit 1
fi

if [ `pwd` != "$NS_WDIR" ];then
	echo "You must be in $NS_WDIR to execute upgrade cmd"
	exit 1
fi

if [ ! -d $NS_WDIR/.rel ];then
	mkdir $NS_WDIR/.rel
fi

echo "Select the NetStorm component to upgrade"
echo "N for NetStorm"
echo "H for HPD"
echo "M for CMON"
echo "G for Gui"
echo "C for Capture"
echo "D for DB"
echo "A for All"
echo "Q for Quit"
echo -n "Make your Selection: "
read INP

while [ $INP != "N" -a $INP != "H" -a $INP != "M" -a $INP != "G" -a $INP != "C" -a $INP != "D" -a $INP != "A" -a $INP != "Q" ]
do
	echo "$INP is not a valid option. Valid options are N, M, H, G, D, C, A or Q"
	echo -n "Make your Selection: "
	read INP
done

if [ $INP = "A" -a "$#" -eq 1 ];then
	echo "No argument may be specified with A option"
	exit 1
fi

if [ $INP = "G" -o $INP = "A" ];then
    if [ "$#" -eq 1 ];then
	GZ="$FILE"
    else
ftp -vin $FHOST >/tmp/upgrade.out <<+
user $FUSER $FPASS
cd $FGDIR
pwd
bin
ls gui*gz
mget gui*gz
y
bye
+

NUM=`grep -c "bytes received" /tmp/upgrade.out`

if [ $NUM -eq 1 ];then
    GZ=`grep "150 Opening BINARY mode data connection for" /tmp/upgrade.out | awk '{print $8}'`
    echo "Upgraded version ($GZ) downloaded"
    echo "Upgrading NetStorm GUI..."
else
    echo "Unable to download latest netstorm GUI. Please Contact Cavisson Systems"
    exit 1
fi
fi

cp $GZ .rel
rm gui*.tar.gz
cd webapps
rm -rf realTimeGraph recorder analyze controller help netstorm .tomcat gui*.tar.gz
cd ..
cp .rel/$GZ webapps
cd webapps
tar xvzf gui*
../bin/nsi_work_setup
cd ..
su -c 'bin/nsi_gui_upgrade'
echo "NetStorm GUI upgrade done"
fi

if [ $INP = "N" -o $INP = "A" ];then
    if [ "$#" -eq 1 ];then
	GZ="$FILE"
    else
ftp -vin $FHOST >/tmp/upgrade.out <<+
user $FUSER $FPASS
cd $FNDIR
pwd
bin
ls netstorm*gz
mget netstorm*gz
y
bye
+

NUM=`grep -c "bytes received" /tmp/upgrade.out`

if [ $NUM -eq 1 ];then
    GZ=`grep "150 Opening BINARY mode data connection for" /tmp/upgrade.out | awk '{print $8}'`
    echo "Upgraded version ($GZ) downloaded"
    echo "Upgrading NetStorm ..."
else
    echo "Unable to download latest netstorm. Please Contact Cavisson Systems"
    exit 1
fi
    fi

cp $GZ .rel
rm netstorm*.tar.gz

rm -rf bin etc include samples
cp .rel/$GZ .
tar xvzf netstorm*tar.gz
bin/nsi_ns_upgrade
echo "netstorm engine upgrade done"
fi

if [ $INP = "H" -o $INP = "A" ];then
    if [ "$#" -eq 1 ];then
	export GZ="$FILE"
    else
ftp -vin $FHOST >/tmp/upgrade.out <<+
user $FUSER $FPASS
cd $FNDIR
pwd
bin
ls hpd*gz
mget hpd*gz
y
bye
+

NUM=`grep -c "bytes received" /tmp/upgrade.out`

if [ $NUM -eq 1 ];then
    export GZ=`grep "150 Opening BINARY mode data connection for" /tmp/upgrade.out | awk '{print $8}'`
    echo "Upgraded version ($GZ) downloaded"
    echo "Upgrading NetOcean ..."
else
    echo "Unable to download latest netocean. Please Contact Cavisson Systems"
    exit 1
fi
    fi

cp $GZ .rel
rm hpd*.tar.gz

su -c 'bin/nsi_hpd_upgrade'
echo "netocean engine upgrade done"
fi

if [ $INP = "M" -o $INP = "A" ];then
    if [ "$#" -eq 1 ];then
	export GZ="$FILE"
    else
ftp -vin $FHOST >/tmp/upgrade.out <<+
user $FUSER $FPASS
cd $FNDIR
pwd
bin
ls cmon*gz
mget cmon*gz
y
bye
+

NUM=`grep -c "bytes received" /tmp/upgrade.out`

if [ $NUM -eq 1 ];then
    export GZ=`grep "150 Opening BINARY mode data connection for" /tmp/upgrade.out | awk '{print $8}'`
    echo "Upgraded version ($GZ) downloaded"
    echo "Upgrading Cavisson Monitors ..."
else
    echo "Unable to download latest netocean. Please Contact Cavisson Systems"
    exit 1
fi
    fi

cp $GZ .rel
rm cmon*.tar.gz

su -c 'bin/nsi_cmon_upgrade'
echo "Cavisson Monitors upgrade done"
fi

if [ $INP = "C" -o $INP = "A" ];then
    if [ "$#" -eq 1 ];then
	GZ="$FILE"
    else
ftp -vin $FHOST >/tmp/upgrade.out <<+
user $FUSER $FPASS
cd $FNDIR
pwd
bin
ls capture*gz
mget capture*gz
y
bye
+

    	NUM=`grep -c "bytes received" /tmp/upgrade.out`
	if [ $NUM -eq 1 ];then
    	    GZ=`grep "150 Opening BINARY mode data connection for" /tmp/upgrade.out | awk '{print $8}'`
    	    echo "Upgraded capture version ($GZ) downloaded"
    	    echo "Upgrading Capture program ..."
	else
    	    echo "Unable to download latest capture programm. Please Contact Cavisson Systems"
    	    exit 1
	fi
    fi

    cp $GZ .rel
    rm capture*.tar.gz

    cd /apps/capture
    rm capture*gz

    cp $NS_WDIR/.rel/$GZ .
                                                                                                                                               
    tar xvzf capture*gz
    ./appy_capture_diff
    ./configure
    gmake
    cd embedding/config
    make
    echo "capture upgrade done"
fi


if [ $INP = "D" -o $INP = "A" ];then
if [ `id -u` -ne 0 ];then
    echo "DB upgrade must be done as root"
    echo "run this command as 'su -c ./upgrade.sh'"
    exit 1
fi
    if [ "$#" -eq 1 ];then
	GZ="$FILE"
    else
ftp -vin $FHOST >/tmp/upgrade.out <<+
user $FUSER $FPASS
cd $FNDIR
pwd
bin
ls dbfunc*gz
mget dbfunc*gz
y
bye
+

NUM=`grep -c "bytes received" /tmp/upgrade.out`
if [ $NUM -eq 1 ];then
    GZ=`grep "150 Opening BINARY mode data connection for" /tmp/upgrade.out | awk '{print $8}'`
    echo "Upgraded DB version ($GZ) downloaded"
    echo "Upgrading DB program ..."
    rm /tmp/upgrade.out
else
    echo "Unable to download latest db programm. Please Contact Cavisson Systems"
    rm /tmp/upgrade.out
    exit 1
fi
    fi

cp $GZ .rel
rm dbfunc*.tar.gz

rm /var/lib/pgsql/dbfunc*.tar.gz

cp .rel/$GZ /var/lib/pgsql

su  postgres -c 'bin/nsi_pg_upgrade'

echo "db upgrade done"
fi

exit 0
