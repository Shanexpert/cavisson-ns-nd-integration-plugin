#!/bin/sh
#nsu_git_service.sh (--server|-s server_name) (--repository|-R repository_name) (--push|-P <NA>) (--pull|-p <NA>) (--clone| -C ) (--files |-f filename
#<f1, f2, f3>) (--commit |-c message) (-add|-a <f1,f2,f3>) (--remove|-r <files|dir>) 


GIT_HOST=""
GIT_REPOSITORY=""
OPERATION=""
FILES=""
COMMIT_MESSAGE=""
GIT_USER=""
SERVICE_DIR=$HPD_ROOT/correlation 
CORRELATION_DIR=netstorm
CLONE_DIR=default/services
CUR_DATE_TIME=`date '+%m%d%y_%H%M%S'` 
CORRELATION_CONF_FILE=$SERVICE_DIR/correlation.conf
GIT_LOGS_DIR=$HPD_ROOT/logs/git/git.log
GIT_ERROR_LOGS_DIR=$HPD_ROOT/logs/git/git_error.log
HPD_CONF_FILE=$HPD_ROOT/conf/hpd.conf
PROJECT="default"
SUB_PROJECT="default"
PRODUCT_NAME="NO"
GIT_PORT=22
DASHBOARD_CONFIG_FILE="$NS_WDIR/webapps/sys/dashboardConfig.properties"

if [ ! -d $HPD_ROOT/logs/git ]; then
  mkdir $HPD_ROOT/logs/git
fi

if [ ! -d $NS_WDIR/logs/git ]; then
  mkdir $NS_WDIR/logs/git
fi

Usage(){
  echo "nsu_git_service
         --Product|-n <product_name> 
         --server|-s server_name 
         --repository|-r repository_name 
         --user|-u <Git_User> 
         --push|-P <NA> 
         --pull|-p <NA>
         --clone|-C <NA>
         --files|-f <f1,f2,f3>
         --commit|-c message
         -add|-a <dir|<f1,f2,f3>
         --remove|-r <files|dir>"
  exit -1;
}        

debug_logs()
{
  echo "$1" >> $GIT_LOGS_DIR
}

error_log_exit()
{
  echo "Error:$1" >> $GIT_ERROR_LOGS_DIR 
  echo "Error:$1" 
  exit -1 
}


#We have to parse GIT_HOST and GIT_CREDENTIALS from hpd.conf
read_hpd_conf()
{
if [ -f $HPD_CONF_FILE ]; then
  grep ^GIT_HOST $HPD_CONF_FILE >/dev/null #spaces should allow at the start of line
  if [ $? -eq 0 ]; then
    GIT_HOST=`grep ^GIT_HOST $HPD_CONF_FILE | cut -d' ' -f2`
    GIT_REPOSITORY=`grep ^GIT_HOST $HPD_CONF_FILE | cut -d' ' -f3`    
  else
    error_log_exit "Keyword GIT_HOST not set in conf file."
  fi  

  grep ^GIT_USER $HPD_CONF_FILE >/dev/null
  if [ $? -eq 0 ]; then
    MODE=`grep ^GIT_USER $HPD_CONF_FILE | cut -d' ' -f2`
    if [ $MODE -eq 2 ]; then    
      GIT_USER=`grep ^GIT_USER $HPD_CONF_FILE | cut -d' ' -f3`
      CORRELATION_DIR=`grep ^GIT_USER $HPD_CONF_FILE | cut -d' ' -f3`
    else
      CORRELATION_DIR=`echo $GIT_USER`
    fi  
  else
    error_log_exit "Keyword GIT_USER not set in conf file."
  fi  

else
  error_log_exit "HPD conf file not found"
fi

}

read_dashboard_config()
{
  if [ -f $DASHBOARD_CONFIG_FILE ]; then
    grep ^GIT_HOST $DASHBOARD_CONFIG_FILE >/dev/null #spaces should allow at the start of line

    if [ $? -eq 0 ]; then
      GIT_HOST=`grep ^GIT_HOST $DASHBOARD_CONFIG_FILE  | cut -d'=' -f2 | cut -d' ' -f2`
      GIT_PORT=`grep ^GIT_HOST $DASHBOARD_CONFIG_FILE  | cut -d' ' -f4`
      CLONE_PATH=`echo $GIT_REPOSITORY`
      GIT_REPOSITORY=`grep ^GIT_HOST $DASHBOARD_CONFIG_FILE  | cut -d' ' -f5`   
      CORRELATION_DIR="$GIT_USER"
    else
      error_log_exit "Keyword GIT_HOST not set in $DASHBOARD_CONFIG_FILE file."
    fi
  fi  
}

#Function to update the value of Keyword CORRELATION_DIR in correlation.conf
update_conf_file()
{
  debug_logs "Updating correlation.conf."
  if [ -f $CORRELATION_CONF_FILE ];then
    sed -i '/^CORRELATION_DIR/d' $CORRELATION_CONF_FILE
    echo "CORRELATION_DIR $CORRELATION_DIR" >> $CORRELATION_CONF_FILE;
  debug_logs "correlation.conf updated."
  else
    debug_logs "$CORRELATION_CONF_FILE configuration file doesn't exist."
  fi
}


#Function to create clone of the repository
clone_repository()
{
  debug_logs "Cloning repository..."
  git clone $GIT_USER@$GIT_HOST:$GIT_REPOSITORY $SERVICE_DIR/$CORRELATION_DIR/$CLONE_DIR >> $GIT_LOGS_DIR
  if [ $? -ne 0 ]; then
    error_log_exit "GIT CLONE Failed."
  else
    debug_logs "Cloning repository...successful"
  fi
}

sparse_checkout()
{
  git config core.sparseCheckout true
  echo "$1/*" >> .git/info/sparse-checkout
  git pull origin master
  if [ $? -ne 0 ]; then
    mv $BACKUP_DIR $SERVICE_DIR/webapps/scripts/$PROJ/$SUB_PROJ/$SCRIPT_DIR 
    error_log_exit "GIT CLONE Failed."
  else
    debug_logs "Cloning repository...successful"
  fi
  
  # To restore checkout to full
  git config core.sparseCheckout false
  git read-tree --empty
  git reset --hard
}

clone_ns_repository()
{
  CLONE_FLAG=0
  debug_logs "Cloning repository..."

  # in case if proj/sub_proj is not present in scenario then script manager is not showing proj/sub/script, hence cloning the
  # scenarios/proj/sub_proj
  echo $CLONE_PATH | grep "scripts"
  if [ $? -eq 0 ];then
    PROJ=`echo $CLONE_PATH | cut -d'/' -f3`
    SUB_PROJ=`echo $CLONE_PATH | cut -d'/' -f4`
    SCRIPT_DIR=`echo $CLONE_PATH | cut -d'/' -f5`
    SCENARIO_DIR="$PROJ/$SUB_PROJ"
    if [ ! -d "$SERVICE_DIR/$SCENARIO_DIR" ]; then
      debug_logs "$SCENARIO_DIR is not present in scenario hence, clonning $SCENARIO_DIR" 
      cd $SERVICE_DIR/scenarios
      git init
      git remote add -f origin $GIT_USER@$GIT_HOST:$GIT_REPOSITORY/scenarios
      sparse_checkout $SCENARIO_DIR
      if [ $? -ne 0 ]; then
        error_log_exit "GIT CLONE Failed for scenarios."
      else
        debug_logs "Cloning repository...successful"
      fi
    fi 
    if [ "X$SCRIPT_DIR" != "X" ]; then
      if [ ! -d "$SERVICE_DIR/webapps/scripts/$PROJ/$SUB_PROJ" ];then
        mkdir -p $SERVICE_DIR/webapps/scripts/$PROJ/$SUB_PROJ
      fi
      cd $SERVICE_DIR/webapps/scripts
      git init
      git remote add -f origin $GIT_USER@$GIT_HOST:$GIT_REPOSITORY/webapps/scripts
      CLONE_DIR="$PROJ/$SUB_PROJ/$SCRIPT_DIR"
      sparse_checkout $CLONE_DIR 
      CLONE_FLAG=1
    fi
  fi

  if [ $CLONE_FLAG -ne 1 ] ;then 
    git clone ssh://$GIT_USER@$GIT_HOST:$GIT_PORT/$GIT_REPOSITORY/$CLONE_PATH $SERVICE_DIR/$CLONE_PATH >> $GIT_LOGS_DIR
    if [ $? -ne 0 ];then
      mv $BACKUP_DIR $SERVICE_DIR/$CLONE_PATH
      error_log_exit "GIT CLONE Failed."
    else
      debug_logs "Cloning repository...successful"
    fi 
  fi
}

#Function for push operation of the repository
push_repository()
{
  if [ "x$PRODUCT_NAME" == "xNO" ];then
    cd $SERVICE_DIR/$CORRELATION_DIR/$CLONE_DIR
  else
    cd $SERVICE_DIR/$CLONE_PATH
  fi
  debug_logs "Pushing to repository..."
  #Before pushing anything to repository we have to perform pull operation
  pull_repository
  git push origin master >> $GIT_LOGS_DIR

  if [ $? -ne 0 ]; then
    error_log_exit "GIT PUSH Failed."
  else
    debug_logs "GIT PUSH...successful"
  fi
}


#Function for pull operartion of the repository
pull_repository()
{
  if [ "x$PRODUCT_NAME" == "xNO" ];then
    cd $SERVICE_DIR/$CORRELATION_DIR/$CLONE_DIR
  else
    cd $SERVICE_DIR/$CLONE_PATH
  fi
  debug_logs "Pulling from repository..."
  pull_result=`git pull`
  if [ $? -ne 0 ]; then
    echo $pull_result | grep CONFLICT > /dev/null
    if [ $? -eq 0 ]; then
      echo $pull_result >> $GIT_ERROR_LOGS_DIR
      #If conflict occurs then reset files
      error_log_exit "CONFLICT in current files...thus resetting."
      git reset --hard >> $GIT_ERROR_LOGS_DIR
      error_log_exit "GIT PULL Failed."
      echo "GIT PULL Failed."
    else
      error_log_exit "GIT PULL Failed."
      echo "GIT PULL Failed."
    fi 
  else
    debug_logs "GIT PULL...successful"
    echo "GIT PULL...successful"
  fi
}


commit_repository()
{
  if [ "x$PRODUCT_NAME" == "xNO" ];then
    cd $SERVICE_DIR/$CORRELATION_DIR/$CLONE_DIR
  else 
    cd $SERVICE_DIR/$CLONE_PATH
  fi
  debug_logs "Adding files..."

  if [ "X$FILES" != "X" ]; then
    git add `echo $FILES | tr ',' ' '` >> $GIT_LOGS_DIR
    if [ $? -ne 0 ]; then
      error_log_exit "GIT ADD Failed."
    else
      debug_logs "Adding files...successful"
    fi
  
    git commit -m "$COMMIT_MESSAGE" `echo $FILES | tr ',' ' '` >> $GIT_LOGS_DIR
    if [ $? -ne 0 ]; then
      error_log_exit "GIT Commit Failed"
    else
      debug_logs "GIT COMMIT...successful"
    fi  

  else
 
    git add . >> $GIT_LOGS_DIR
    if [ $? -ne 0 ];then
      error_log_exit "GIT ADD Failed."
    else
      debug_logs "Adding files...successful"
    fi
  
    git commit -m "$COMMIT_MESSAGE" >> $GIT_LOGS_DIR
    if [ $? -ne 0 ]; then
      error_log_exit "GIT Commit Failed"
    else
      debug_logs "GIT COMMIT...successful"
    fi

  fi

  push_repository
}

#MAIN()
debug_logs "#####################################################################" 

while true; do
  case "$1" in
    -s | --server ) GIT_HOST="$2"; shift 2 ;;
    -r | --repository ) GIT_REPOSITORY="$2"; shift 2 ;;
    -u | --user ) GIT_USER="$2"; shift 2 ;;
    -P | --push ) OPERATION="PUSH"; shift  ;;
    -p | --pull ) OPERATION="PULL"; shift ;;
    -C | --clone ) OPERATION="CLONE"; shift 1 ;;
    -c | --commit )OPERATION="COMMIT"; COMMIT_MESSAGE="$2"; shift 2  ;;
    -f | --files ) FILES="$2"; shift 2 ;;
    -a | --add ) OPERATION="ADD"; FILES="$1"; shift ;;
    -r | --remove )OPERATION="REMOVE"; FILES="$1"; shift ;;
    -n | --Product ) PRODUCT_NAME="$2"; shift 2;;
    -- ) shift; break ;;
    * )   break ;;
  esac
done 

if [ "X$OPERATION" == "X" ]; then
  Usage
fi

if [ "x$PRODUCT_NAME" != "xNO" -a "x$PRODUCT_NAME" != "xNS" ];then
  echo "Product is either NS or NO"
fi

if [ "x$PRODUCT_NAME" == "xNO" ];then
  read_hpd_conf
  CORRELATION_DIR=$GIT_USER
else
  read_dashboard_config
  GIT_USER=cavisson
  SERVICE_DIR="$NS_WDIR"
  GIT_LOGS_DIR=$NS_WDIR/logs/git/git.log
  GIT_ERROR_LOGS_DIR="$NS_WDIR/logs/git/git_error.log"
fi

debug_logs "Parsing done."
debug_logs "GIT_user=$GIT_USER, Git_server=$GIT_HOST, Repository=$GIT_REPOSITORY, Operation=$OPERATION, Files=$FILES, Commit_message=$COMMIT_MESSAGE, Service_Dir=$SERVICE_DIR, Correlation_Dir=$CORRELATION_DIR, Clone_dir=$CLONE_DIR"


if [ "X$OPERATION" == "XCLONE" ]; then
  if [ "X$GIT_HOST" == "X" ]; then
    error_log_exit "Error: Enter the GIT_HOST." 
   
  elif [ "X$GIT_REPOSITORY" == "X" ]; then
      error_log_exit "Error: Enter the GIT_REPOSITORY." 
  fi

  if [ "x$PRODUCT_NAME" == "xNO" ];then 
    if [ ! -d  "$SERVICE_DIR/$CORRELATION_DIR" ]; then
      debug_logs "Correlation Directory is not present, creating new directory..."
      mkdir -p $SERVICE_DIR/$CORRELATION_DIR/$CLONE_DIR
      debug_logs "Directory is created."
      update_conf_file 
      clone_repository 
    elif [ "$( ls -A $SERVICE_DIR/$CORRELATION_DIR)" ]; then
      debug_logs "Directory already present creating backup in $SERVICE_DIR/$CORRELATION_DIR`echo _`$CUR_DATE_TIME "
      mv $SERVICE_DIR/$CORRELATION_DIR $SERVICE_DIR/$CORRELATION_DIR`echo _`$CUR_DATE_TIME     
      clone_repository
    else
      clone_repository
    fi 
  else
    #This is the case of NS
    if [ ! -d  "$SERVICE_DIR/$CLONE_PATH" ]; then
      debug_logs "$SERVICE_DIR/$CLONE_PATH is not present, creating new directory..."
      mkdir -p $SERVICE_DIR/$CLONE_PATH
      debug_logs "Directory is created."
    elif [ "$( ls -A $SERVICE_DIR/$CLONE_PATH)" ]; then
      echo $CLONE_PATH | grep "scripts"
      if [ $? -eq 0 ];then
        PROJ=`echo $CLONE_PATH | cut -d'/' -f3`
        SUB_PROJ=`echo $CLONE_PATH | cut -d'/' -f4`
        SCRIPT_DIR=`echo $CLONE_PATH | cut -d'/' -f5`
        if [ "x$PROJ" == "x" ];then
          BACKUP_DIR="$SERVICE_DIR/webapps/.scripts`echo _`$CUR_DATE_TIME"
          # This is the case of cloning webapps/scripts
          mv $SERVICE_DIR/$CLONE_PATH $BACKUP_DIR
        else
          BACKUP_DIR=$SERVICE_DIR/webapps/scripts/$PROJ/$SUB_PROJ/.$SCRIPT_DIR`echo _`$CUR_DATE_TIME
          mv $SERVICE_DIR/$CLONE_PATH $BACKUP_DIR
        fi
      else
        BACKUP_DIR="$SERVICE_DIR/.$CLONE_PATH`echo _`$CUR_DATE_TIME"
        debug_logs "Directory already present creating backup in $SERVICE_DIR/.$CLONE_PATH`echo _`$CUR_DATE_TIME"
        mv $SERVICE_DIR/$CLONE_PATH $BACKUP_DIR 
      fi
    fi
      clone_ns_repository 
  fi
fi

if [ "X$OPERATION" == "XPUSH" ] ; then
  push_repository ;
fi

if [ "X$OPERATION" == "XPULL" ] ; then
   pull_repository ;
fi 
          
if [ "X$OPERATION" == "XCOMMIT" ] && [  "X$COMMIT_MESSAGE" != "X" ]; then
  commit_repository ;
fi

