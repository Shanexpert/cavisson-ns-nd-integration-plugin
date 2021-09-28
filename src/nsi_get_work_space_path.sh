# Name      : nsi_get_work_space_path.sh
# Author    : Somin Dosi
# Purpsose  : It use for find the workspace path and test_assests path.
# Usage     : nsi_get_work_space_path.sh workspace/profile proj/sub_proj/test_assests
#           : ex -: nsi_get_work_space_path.sh admin/default
#           : ex -: nsi_get_work_space_path.sh admin/default proj/sub_proj/test_assests
# Modification History:
# 17/03/21: Somin Dosi - Initial version
#!/bin/sh

#*********************************************************************#
#                       Define all variables                          #
#*********************************************************************#
#default workspace
DW="admin"
#default profile
DP="default"
WORKSPACE=""
PROFILE=""
#WORK_PROFILE ==> WORKSPACE + PROFILE
WORK_PROFILE=""
NS_TA_DIR=""
NS_RTA_DIR="workspace"
PROJ=""
SUB_PROJ=""
TEST_ASSESTS=""
#**********************************************************************# 

#**********************************************************************#
#               Set Project, Subproject and Test assests               #
#**********************************************************************#
#i/p would be <prj>/<subproj>/<test_assets name>
set_proj_subproj_test_asset()
{
   PROJ=`echo ${1} | egrep "/" | cut -d'/' -f1`
   SUB_PROJ=`echo ${1} | egrep "/" | cut -d'/' -f2`
   TEST_ASSESTS=`echo ${1} | egrep "/" | cut -d'/' -f3`
   if [ "X$TEST_ASSESTS" == "X" ];then   
      NS_TA_DIR=$NS_TA_DIR/$PROJ/$SUB_PROJ
   else
      NS_TA_DIR=$NS_TA_DIR/$PROJ/$SUB_PROJ/$TEST_ASSESTS
   fi
}
#***********************************************************************#
 
#************************************************************************************************#
#                           Check and Get Workspace and Profile                                  #
#************************************************************************************************#
#check for workspace/profile name and RetlativeTestAsset Dir
check_and_set_workspace_profile()
{
  #if empty aregument passed
  if [ \( "X${WORK_PROFILE}" == "X" \) -o \( "X${WORK_PROFILE}" == "${DW}/${DP}" \) ];then
   WORKSPACE=$DW
   PROFILE=$DP
   WORK_PROFILE=${DW}/${DP}
   return 1
  fi

  #if default workspace/profile
   
  WORKSPACE=`echo $WORK_PROFILE | egrep "/" | cut -d'/' -f1`
  PROFILE=`echo $WORK_PROFILE | egrep "/" | cut -d'/' -f2`

  #<workspace> i.e. profile no given
  #<workspace>/ i.e. profile no given
  #/<profile> i.e. profile no given
  if [ "X$WORKSPACE" == "X" ];then
     #set default value
     WORKSPACE=$DW
  fi

  if [ "X$PROFILE" == "X" ];then
     #set default value
     PROFILE=$DP
  fi
  #if WORKSPACE is NOT default workspace
  if [ $WORKSPACE != $DW ];then
     #check if workspace exists
     if [ ! -d ${NS_WDIR}/${NS_RTA_DIR}/$WORKSPACE ];then
       #set to default workspace
       WORKSPACE=$DW
     fi
  fi
  #check if profile exists
  if [ ! -d ${NS_WDIR}/${NS_RTA_DIR}/$WORKSPACE/$PROFILE ];then
      #set to default workspace
      PROFILE=$DP
  fi

  #set workspace and profile path
  WORK_PROFILE=${WORKSPACE}/${PROFILE}
}

#set relative test assets dir
set_rta_dir()
{
  NS_RTA_DIR="workspace/$1/$2/cavisson"
}
#************************************************************************************************#

#***********************************************************************************************#
#                              Execution Start form here                                        # 
#***********************************************************************************************#
WORK_PROFILE=$1
#-w anup/ns_dev_4.6
#-w <workspace_name>/<profile_name>

check_and_set_workspace_profile $WORK_PROFILE
set_rta_dir $WORKSPACE $PROFILE
#set absolute test assets path
NS_TA_DIR=$NS_WDIR/$NS_RTA_DIR
if [ "XX${2}" != "XX" ];then
   set_proj_subproj_test_asset ${2}
fi
echo $NS_TA_DIR
#************************************************************************************************#
