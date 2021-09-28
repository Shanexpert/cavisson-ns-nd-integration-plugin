tmpdir=$NS_WDIR/server_management/tmp
errorlog=$NS_WDIR/server_management/logs/ping_CC_err.log
if [ ! -d $tmpdir ];
then
 echo "$tmpdir does not exist. Exiting" >$2
 exit 1
fi

ping -i 5 -c 30 -W 5 $1 >/dev/null 2>&1
EXIT_STATUS=$?
if [ $EXIT_STATUS -eq 0 ]
then
        echo ON > $tmpdir/.$2.log
else
        echo OFF > $tmpdir/.$2.log
fi
