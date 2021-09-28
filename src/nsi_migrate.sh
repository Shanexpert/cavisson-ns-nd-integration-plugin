#!/bin/sh

#this shell migrates older version to current version(3.2.0), currently change RAMPDOWN => RAMP_DOWN_MODE, & removes cm_vmstat.gdf 

#NS_VER_BLD is calculated as 3.2.0

#gets the current cavisson of system, change it like 3.1.0 => 310
#Migration from pre 3.2.0

func_migrate_from_pre_3_2_0()
{
  # Remove cm_vmstat.gdf as it has changed to cm_vmstat_linux.gdf 
  rm -f $NS_WDIR/sys/cm_vmstat.gdf

  ls -1 $SCENARIOS_DIR >/tmp/scenario.lst

  while read scen_name
  do
    RAMPDOWN_FOUND=`grep -cw "^RAMPDOWN" $SCENARIOS_DIR/$scen_name` 
    RAMP_DOWN_MODE_FOUND=`grep -cw "^RAMP_DOWN_MODE" $SCENARIOS_DIR/$scen_name` 
 
    #if RAMP_DOWN_MODE is not there & RAMPDOWN is there, only then replace the RAMPDOWN keyword
    if [ $RAMPDOWN_FOUND == 0 -o $RAMP_DOWN_MODE_FOUND != 0 ];then
     continue;
    fi

    cp $SCENARIOS_DIR/$scen_name /tmp/$scen_name.tmp
    su cavisson -c "sed 's/RAMPDOWN/RAMP_DOWN_MODE 0/g' /tmp/$scen_name.tmp >$SCENARIOS_DIR/$scen_name"
    rm -f /tmp/$scen_name.tmp

  done < /tmp/scenario.lst

  rm -f /tmp/scenario.lst
}

#Migration from pre 3.2.0
NS_VER_BLD=320
#this was project/subproject
NS_VER_BLD_PROJ=323
NS_VER_BLD_GROUP=340   #GROUP Based changes
NS_VER_BLD_TCL=341     #TEST_CYCLE impimentation for Automation
SCENARIOS_DIR="$NS_WDIR/scenarios/"
NS_COMPONENT_INSTALLATION_DONE=$1
NS_VER_BLD_SCHEDULE=350   #Schedule Based changes
NS_VER_BLD_370=370
NS_VER_BLD_374=374

NS_VER_BLD_385=385 #For corelation changes
NS_VER_BLD_415=415 
NS_VER_BLD_416=416
NS_VER_BLD_417=417
NS_VER_BLD_4_1_11=4111
BUILD=43
BUILD_SERVER_HOST=15
NS_VER_BLD_392=392 #For NDC
NS_VER_BLD_4111=4111
NS_VER_BLD_4112=4112
BUILD_4111=15
BUILD_4_1_12_8=8

if [ ! -d $NS_WDIR/bin ];then
   exit 0
fi

if [ "X$NS_COMPONENT_INSTALLATION_DONE" == "X0" -a "$NS_VER_SYS" -lt "$NS_VER_BLD" ];then 
  #commented in 3.4.0 release
  #func_migrate_from_pre_3_2_0
  :
fi

#This code is commented because no one uses build < 3.2.0, later on we will delete it

#if [ "$NS_COMPONENT_INSTALLATION_DONE" == "1" -a "$NS_VER_SYS" -lt "$NS_VER_BLD_PROJ" ];then
##  su cavisson -c "mkdir -p $NS_WDIR/mprof"
 # su cavisson -c "mkdir -p $NS_WDIR/scenarios/default/default"
 # su cavisson -c "mkdir -p $NS_WDIR/scripts/default/default"
 # echo "Copying scenarios & scripts to default/default & monitor profiles to mprof directory..."
 # echo "This may take few minutes"
 # mv $NS_WDIR/scenarios/*.conf $NS_WDIR/scenarios/default/default/
 # mv $NS_WDIR/scenarios/*.save $NS_WDIR/scenarios/default/default/
 # MPROF_ENT=`ls $NS_WDIR/scenarios/*.mprof 2>/dev/null`
 # if [ "XX$MPROF_ENT" != "XX" ];then
 #   mv $NS_WDIR/scenarios/*.mprof $NS_WDIR/mprof/
 #   chmod 664 $NS_WDIR/mprof/*
 # fi
 # SCRIPTS_TO_COPY=`ls -1 $NS_WDIR/scripts/ | egrep -wv "default|WEB-INF"`
 # cd $NS_WDIR/scripts/
 # mv $SCRIPTS_TO_COPY $NS_WDIR/scripts/default/default/
#fi
if [ "X$NS_COMPONENT_INSTALLATION_DONE" ==  "X1"  -a "$NS_VER_SYS" -lt "$NS_VER_BLD_415" ];then

  #tar cvzf /tmp/scenario_${NS_VER_SYS}.tar.gz $NS_WDIR/scenarios >/dev/null 2>&1 
  #tar cvzf /tmp/mprof_${NS_VER_SYS}.tar.gz $NS_WDIR/mprof        >/dev/null 2>&1

  echo "Migrating G_CONTINUE_ON_PAGE_ERROR keyword"
  chmod +x $NS_WDIR/bin/nsi_migrate_scen
  $NS_WDIR/bin/nsi_migrate_scen -a -r $NS_VER_SYS 
  if [ $? != 0 ];then
    echo "Migration of Old keyword failed."
  else
    echo "Migration of Old keyword done."
  fi

fi

if [ "X$NS_COMPONENT_INSTALLATION_DONE" ==  "X1"  -a "$NS_VER_SYS" -lt "$NS_VER_BLD_416" ];then

  #tar cvzf /tmp/scenario_${NS_VER_SYS}.tar.gz $NS_WDIR/scenarios >/dev/null 2>&1 
  #tar cvzf /tmp/mprof_${NS_VER_SYS}.tar.gz $NS_WDIR/mprof        >/dev/null 2>&1

  echo "Migrating RAMP_DOWN_METHOD keyword"
  chmod +x $NS_WDIR/bin/nsi_migrate_scen
  $NS_WDIR/bin/nsi_migrate_scen -a -r $NS_VER_SYS 
  if [ $? != 0 ];then
    echo "Migration of Old keyword failed."
  else
    echo "Migration of Old keyword done."
  fi

fi

if [ "X$NS_COMPONENT_INSTALLATION_DONE" ==  "X1"  -a "$NS_VER_SYS" -lt "$NS_VER_BLD_417" ];then
  echo "Migrating G_OVERRIDE_RECORDED_THINK_TIME keyword"
  chmod +x $NS_WDIR/bin/nsi_migrate_scen
  $NS_WDIR/bin/nsi_migrate_scen -a -r $NS_VER_SYS -b $NS_VER_BUILD 
  if [ $? != 0 ];then
    echo "Migration of Old keyword failed."
  else
    echo "Migration of Old keyword done."
  fi
elif [ "X$NS_COMPONENT_INSTALLATION_DONE" ==  "X1"  -a "$NS_VER_SYS" -eq "$NS_VER_BLD_417" -a $NS_VER_BUILD -lt $BUILD ];then
  echo "Migrating G_OVERRIDE_RECORDED_THINK_TIME keyword"
  chmod +x $NS_WDIR/bin/nsi_migrate_scen
  $NS_WDIR/bin/nsi_migrate_scen -a -r $NS_VER_SYS -b $NS_VER_BUILD 
  if [ $? != 0 ];then
    echo "Migration of Old keyword failed."
  else
    echo "Migration of Old keyword done."
  fi
fi

if [ "X$NS_COMPONENT_INSTALLATION_DONE" ==  "X1"  -a "$NS_VER_SYS" -lt "$NS_VER_BLD_4111" ];then
  echo "Migrating USE_SRC_IP, SRC_IP_LIST, USE_SAME_NETID_SRC, IP_VERSION_MODE keyword"
  chmod +x $NS_WDIR/bin/nsi_migrate_scen
  $NS_WDIR/bin/nsi_migrate_scen -a -r $NS_VER_SYS -b $NS_VER_BUILD 
  if [ $? != 0 ];then
    echo "Migration of Old keyword failed."
  else
    echo "Migration of Old keyword done."
  fi
elif [ "X$NS_COMPONENT_INSTALLATION_DONE" ==  "X1"  -a "$NS_VER_SYS" -eq "$NS_VER_BLD_4111" -a $NS_VER_BUILD -lt $BUILD_4111 ];then
  echo "Migrating USE_SRC_IP keyword"
  chmod +x $NS_WDIR/bin/nsi_migrate_scen
  $NS_WDIR/bin/nsi_migrate_scen -a -r $NS_VER_SYS -b $NS_VER_BUILD 
  if [ $? != 0 ];then
    echo "Migration of Old keyword failed."
  else
    echo "Migration of Old keyword done."
  fi
fi
#This code is commented because no one uses build < 3.4.1, later on we will delete it
#if [ "$NS_COMPONENT_INSTALLATION_DONE" == "1" -a "$NS_VER_SYS" -lt "$NS_VER_BLD_TCL" ];then
#  echo "Migrating Old Test Suite Numbers to directory 000000_000000"
#  CUR_DIR=`pwd`
#  TSR_DIR=$NS_WDIR/logs/tsr/
#  cd $TSR_DIR
#  TO_MOVE=`ls [1-9]* 2>/dev/null`
#  if [ "X$TO_MOVE" != "X" ];then
#    mkdir -p 000000_000000
#    mv $TO_MOVE 000000_000000/
#  fi
#  if [ $? != 0 ];then
#    echo "Migration of old test suite numbers to directory 000000_000000 failed"
#  else
#    echo "Migration of old test suite numbers to directory 000000_000000 done"
#  fi
#  cd $CUR_DIR
#fi

#Migration of site.env file
if [ "X$NS_COMPONENT_INSTALLATION_DONE" ==  "X1" -a "$NS_VER_SYS" -lt "$NS_VER_BLD_SCHEDULE" ];then

  echo "Migrating keyword TOMCAT_OPTS of site.env file"
  SITE_ENV_FILE="$NS_WDIR/sys/site.env"

  if [ -f $SITE_ENV_FILE ]; then
    egrep "TOMCAT_OPTS" $SITE_ENV_FILE > /dev/null
    if [ $? != 0 ]; then  #TOMCAT_OPTS  not found, therefore add the default value -Xmx200m
      echo "export TOMCAT_OPTS=\"-Xmx200m\"" >>$SITE_ENV_FILE
    fi # $? end
      echo "Migration of TOMCAT_OPTS keyword is done."
  else
    echo "site.env does not exist in $NSWDIR/sys directory."
    echo "Keyword TOMCAT_OPTS of site.env Migration failed"
  fi # file exist check end

fi

# Whats the role of nsi_get_linux_release ? 
# => It is used to validate on FC4/FC9 builds on machine.
#    This check is added because we changed nsi_get_linux_release
#    which is picked and used from /home/cavisson/work/tools
#    nsi_upgrade.sh before 3.7.8 release. 
#    But now we bundle with the bin & taken from cur dir 
#    but now some changes are done in nsi_get_linux_release so
#    when we upgrade
#    nsi_get_linux_release is upgraded & when we downgrade build
#    this changed nsi_get_linux_release is picked hence ISSUE smiles.

if [ "X$NS_COMPONENT_INSTALLATION_DONE" ==  "X0" ];then
   if [ -f $NS_WDIR/tools/nsi_get_linux_release ];then 
     cp $NS_WDIR/tools/nsi_get_linux_release /tmp/
   fi
else
   if [ -f /tmp/nsi_get_linux_release ];then 
     mv /tmp/nsi_get_linux_release $NS_WDIR/tools/
   fi
fi

#For HPD correlation 
#If build is less then 3.8.5 then this code is execute 
if [ "X$NS_COMPONENT_INSTALLATION_DONE" ==  "X1" -a "$NS_VER_SYS" -lt "$NS_VER_BLD_385" ];then
  #If HPD_ROOT is not set set as work
  if [ "X$HPD_ROOT" = "X" ];then
    HPD_ROOT=$NS_WDIR/hpd
  fi
  #Make tar for old correlated directory and run Java class 
  tar cvzf $HPD_ROOT/correlation_${NS_VER_SYS}.tar.gz $HPD_ROOT/correlation >/dev/null 2>&1 
  java -cp $NS_WDIR/webapps/cavisson/WEB-INF/lib/netstorm_bean.jar  -DHPD_ROOT=$HPD_ROOT -DNS_WDIR=$NS_WDIR pac1.Bean.CorrelationService $* -u $User_NAME
fi

# For NDC 
# If build is less then 3.9.2 then this code is execute 
if [ "X$NS_COMPONENT_INSTALLATION_DONE" ==  "X0" -a "$NS_VER_SYS" -lt "$NS_VER_BLD_392" ];then
  OS_NAME=`uname`
  if [ "X$OS_NAME" == "XSunOS" ]; then
    PS_CMD_FOR_SEARCH="/usr/ucb/ps -auxwww"
  else #Linux,AIX
    PS_CMD_FOR_SEARCH="ps -ef" # Do not use ps -lef as need pid at filed 2
  fi

  PID=`$PS_CMD_FOR_SEARCH | grep -w  $NS_WDIR/lps/bin/ndcollector | grep -v "grep" | grep -v "cm_ps_data" | awk '{print $2}'`
  #In some unix machine, ps -ef does not give complete output
  if [ "XX$PID" == "XX" ];then
    PID=`$PS_CMD_FOR_SEARCH | grep "\-DPKG= $NS_WDIR/lps/bin/ndcollector" | grep -v "grep" | awk '{print $2}'`
  fi
  
  if [ "XX$PID" != "XX" ];then
    kill -9 $PID
  fi
fi

#changes for g_server_host
if [ "X$NS_COMPONENT_INSTALLATION_DONE" ==  "X1"  -a "$NS_VER_SYS" -lt "$NS_VER_BLD_4_1_11" ];then
  echo "migrating server_host keyword"
  chmod +x $NS_WDIR/bin/nsi_migrate_scen
  $NS_WDIR/bin/nsi_migrate_scen -a -r $NS_VER_SYS -b $NS_VER_BUILD 
  if [ $? != 0 ];then
    echo "migration of old keyword failed."
  else
    echo "migration of old keyword done."
  fi
elif [ "X$NS_COMPONENT_INSTALLATION_DONE" ==  "X1"  -a "$NS_VER_SYS" -eq "$NS_VER_BLD_4_1_11" -a $NS_VER_BUILD -lt $BUILD_SERVER_HOST ];then
  echo "migrating server_host keyword"
  chmod +x $NS_WDIR/bin/nsi_migrate_scen
  $NS_WDIR/bin/nsi_migrate_scen -a -r $NS_VER_SYS -b $NS_VER_BUILD 
  if [ $? != 0 ];then
    echo "migration of old keyword failed."
  else
    echo "migration of old keyword done."
  fi
fi

#changes for Group based SSL settings
if [ "X$NS_COMPONENT_INSTALLATION_DONE" ==  "X1"  -a "$NS_VER_SYS" -lt "$NS_VER_BLD_4112" ];then
  echo "migrating Group based SSL settings keyword"
  chmod +x $NS_WDIR/bin/nsi_migrate_scen
  $NS_WDIR/bin/nsi_migrate_scen -a -r $NS_VER_SYS -b $NS_VER_BUILD
  if [ $? != 0 ];then
    echo "migration of old keyword failed."
  else
    echo "migration of old keyword done."
  fi
elif [ "X$NS_COMPONENT_INSTALLATION_DONE" ==  "X1"  -a "$NS_VER_SYS" -eq "$NS_VER_BLD_4112" -a $NS_VER_BUILD -lt $BUILD_4_1_12_8 ];then
  echo "migrating server_host keyword"
  chmod +x $NS_WDIR/bin/nsi_migrate_scen
  $NS_WDIR/bin/nsi_migrate_scen -a -r $NS_VER_SYS -b $NS_VER_BUILD
  if [ $? != 0 ];then
    echo "migration of old keyword failed."
  else
    echo "migration of old keyword done."
  fi
fi

#changes for migrating cshm conf
if [ "X$NS_COMPONENT_INSTALLATION_DONE" ==  "X1"  -a "$NS_VER_SYS" -lt "$NS_VER_BLD_4112" -a "$NS_WDIR" = "/home/cavisson/work" ];then
   echo "migrating cshm conf"
   chmod +x $NS_WDIR/bin/nsi_migrate_cshm_conf
   $NS_WDIR/bin/nsi_migrate_cshm_conf
   if [ $? != 0 ];then
     echo "migration of old cshm conf failed."
   else
     echo "migration of old cshm conf done."
   fi
fi
exit 0
