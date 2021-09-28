# Name      : nsu_get_ta_dir
# Author    : Somin Dosi
# Purpsose  : It use for moving test assest at the time of build upgrade.
# Usage     : nsi_build_upgrade_multipleworkspace.sh 
# Modification History:
# 17/03/21: Somin Dosi - Initial version
#!/bin/sh

DATA_DIR="data"
NS_SP_DIR="${NS_WDIR}/workspace/admin/system"
NS_DP_DIR="default"
echo $NS_WDIR
#NS_RP_DIR="${NS_WDIR}/workspace/admin/repo"
NS_WP_DIR=
NS_TA_DIR=
#CONFIG_FILE="${NS_WDIR}/webapps/sys/dashboardConfig.properties"
#LOCAL_REPO_BASE="${NS_WDIR}/.gitrepo"
READ_LINE=""
REPO_PATH=""
REPO_NAME=""
DES_PATH=""
SHARED_PATH=""
TA_DIR=""
CORRELATION="correlation"
TA_ARR=("scenarios" "scripts" "testcases" "testsuites"  "checkprofile" "scenario_profiles" "replay_profiles" "ReplayAccessLogs" "services")
FORCE_MODE=0

if [ "X$1" == "X-f" ];then
  FORCE_MODE=1
fi

debug_log() 
{
   echo $*
   echo $* >>$NS_WDIR/.migration.log
}


move_specified_test_assets()
{
  dir=""
  if [ -d ${NS_WDIR}/$1 ];then
    cd ${NS_WDIR}/$1
    TA_DIR=$1
    PROJ_SUB_PROJ=`ls -dl */*/|awk -F " " '{dest="'${NS_TA_DIR}'"$NF"'$TA_DIR'";print dest;cmd="mkdir -p "dest;system(cmd);cmd1="mv '${NS_WDIR}'/'$TA_DIR'/"$NF"* "dest;system(cmd1)}'`

  if [ $? -eq 0 ]; then  
    dir=${NS_WDIR}/$1
    #If directory is empty then directory will be deleted.
    IS_EMPTY=`ls -A ${dir} |wc -l`
    if [ $IS_EMPTY -ne 0 ];then
      debug_log "Failed to move all assets from ${dir} to ${NS_TA_DIR}"
      return
    fi
    debug_log "All assets moved from ${dir} to ${NS_TA_DIR}. Hence removing ${dir}"
    rm -rf ${dir}
    if [ "X$1" == "Xscripts" ];then
      dir=${NS_WDIR}/webapps/$1
      rm -rf ${dir}
    fi
  else 
    debug_log "Test asset $1 not found"
  fi
 fi
}
move_test_assets()
{
   debug_log "moving Test Assets"
   for i in "${TA_ARR[@]}"
   do 
        debug_log "moving Test Asset $i"
	move_specified_test_assets $i 
   done
}

work_profile_migration()
{
  if [ -d $NS_SP_DIR -a $FORCE_MODE -eq 0 ];then
    debug_log "Migration aborting!!! either system or repo  dir already exists at ${NS_WDIR}/workspace/admin."
    exit
  fi

  if [ $FORCE_MODE -eq 1 ];then
    read -p "Controller is already migrated. Do you want forcefully continue? (y/n): " confirm && [[ $confirm == [yY] ]] || exit 1
  fi
  debug_log "Starting NS Test Assets migration"
  debug_log "Migrating Test Assets from $NS_WDIR"
  #create system profile
  debug_log "Creating system profile"
  mkdir -p ${NS_SP_DIR}/.logs
  touch ${NS_SP_DIR}/.workprofile.conf
  chmod 755  $NS_SP_DIR/.workprofile.conf
  for i in "${TA_ARR[@]}"
  do 
    mkdir -p $NS_SP_DIR/cavisson/default/default/$i 
  done
  debug_log "system profile created"
  NS_WP_DIR=${NS_SP_DIR}

  #if [ -d $LOCAL_REPO_BASE ];then
  #  debug_log "Migrating Test Assets from git repo"
  #  #create repo profile
  #  mkdir -p $NS_RP_DIR/.logs
  #  touch $NS_RP_DIR/.workprofile.conf
  #  chmod 755 $NS_RP_DIR/.workprofile.conf
  #  debug_log " repo dir creation $NS_RP_DIR"
  #  #set work profile
  #  NS_WP_DIR=${NS_RP_DIR}
  
  #  READ_LINE=`grep "^GIT_HOST.*=" ${CONFIG_FILE} | cut -d'=' -f2-`
  #  REPO_PATH=`echo ${READ_LINE} | cut -d' ' -f3` 
  #  REPO_NAME=`echo ${REPO_PATH} | awk -F'/' '{print $NF}' | cut -d'.' -f 1`
     
  #  debug_log "repo path = $REPO_PATH, repo name = $REPO_NAME from config file $CONFIG_FILE "
  #  DES_PATH=`ls -l $LOCAL_REPO_BASE|awk '{print $NF}'`
  # 
  #  if [ -L ${LOCAL_REPO_BASE} ];then
  #    debug_log "${LOCAL_REPO_BASE} is a link"
  #    SHARED_PATH=${DES_PATH}/../
  # 
  #    mv ${DES_PATH}/${REPO_NAME}  ${SHARED_PATH} 
     
  #    ln -sf ${SHARED_PATH}/${REPO_NAME}/cavisson $NS_WP_DIR/ 2>/dev/null
  # 
  #    rm -rf ${DES_PATH}
  #    rm -rf ${LOCAL_REPO_BASE}
  #  else
  #    mv ${LOCAL_REPO_BASE}/${REPO_NAME}/cavisson ${NS_WP_DIR}/
  #    rm -rf ${LOCAL_REPO_BASE}
  #  fi
  #fi

  #set NS_TA_DIR
  NS_TA_DIR=$NS_WP_DIR/cavisson/
  debug_log "Test Assets dir = $NS_TA_DIR"
  #creating data dir.
  DATA_DIR_PATH="${NS_TA_DIR}/${DATA_DIR}"
  mkdir -p  ${DATA_DIR_PATH}/shared
  mkdir -p  ${DATA_DIR_PATH}/tmp
  #if [ "X$NS_WP_DIR" == "X$NS_RP_DIR" ]; then
  #  echo ".*" > ${NS_TA_DIR}/.gitignore
  #  echo "$DATA_DIR" >> ${NS_TA_DIR}/.gitignore
  #  echo "correleation.conf" >> ${NS_TA_DIR}/.gitignore
  #  chmod 755 ${NS_TA_DIR}/.gitignore
  #  echo ".*" > ${DATA_DIR_PATH}/.gitignore
  #  chmod 755 ${DATA_DIR_PATH}/.gitignore
  #fi
  debug_log "data  dir created at = $DATA_DIR_PATH"
  #create default profile
  NS_WORK_PROFILE=`basename ${NS_WP_DIR}`
  cd $NS_WDIR/workspace/admin;ln -nfs ${NS_WORK_PROFILE} ${NS_DP_DIR}
  debug_log "ln -nfs ${NS_WORK_PROFILE} ${NS_DP_DIR}"

  #Moving all test assests
  move_test_assets 

  cd $NS_WDIR/webapps;ln -nfs $NS_WDIR/workspace workspace
  debug_log "ln -nfs $NS_WDIR/workspace workspace" 
  debug_log "NS Test Assets migrated successfully"
}

migrate_no_profile()
{
  if [ ! -d $HPD_ROOT/$CORRELATION ];then
     debug_log "$CORRELATION dir doesn't exists at $HPD_ROOT/$CORRELATION"
     return
  fi
 
  debug_log "Starting NO Test Assets migration"
 
  #copy $HPD_ROOT/correlation to $NS_TA_DIR
  cp -rf $HPD_ROOT/$CORRELATION/* $NS_TA_DIR/ 
  cp -rf $HPD_ROOT/$CORRELATION/.system $NS_TA_DIR/
 
  #remove $HPD_ROOT/correlation
  rm -rf $HPD_ROOT/$CORRELATION
 
  #create soft link $NS_TA_DIR to $HPD_ROOT/correlation
  #cd $HPD_ROOT; ln -nfs $NS_TA_DIR $CORRELATION
  cd $HPD_ROOT;  ln -nsf ../workspace/admin/system/cavisson $CORRELATION
 
  debug_log "NO Test Assets migrated successfully"
}
########################################################################################
#                        Execution starts from here                                    #
########################################################################################

work_profile_migration

migrate_no_profile
