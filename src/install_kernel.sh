#!/bin/sh

export IDIR=`pwd`
LOG_FILE_NAME="$IDIR/cav_kernel_install.log"
> $LOG_FILE_NAME

msgout ()
{
  echo "$1" >>$LOG_FILE_NAME
  echo "$1"
}

USERID=`id -u`
if [ $USERID -ne 0 ];then
  msgout "You must be root to execute this command"
  exit -1
fi


function install_cav_kernel()
{
  KERNEL_NAME=`ls linux*tar.gz`
  LINUX_KERNEL_VERSION=`echo $KERNEL_NAME | cut -d "-" -f2 | cut -d "." -f1-3`
  CAV_KERNEL_VERSION=`echo $KERNEL_NAME | cut -d "-" -f2 | cut -d "." -f4`

  msgout "----------------------------------------------------------------------"
  msgout "Installing Cavisson Kernel $LINUX_KERNEL_VERSION.$CAV_KERNEL_VERSION"
  msgout "This will take approximately 30 to 45 minutes ........."

  cp $IDIR/linux-$LINUX_KERNEL_VERSION.$CAV_KERNEL_VERSION.tar.gz /usr/src/

  cd /usr/src/

  rm -rf linux-$LINUX_KERNEL_VERSION.$CAV_KERNEL_VERSION.configured

  echo "Untaring linux tar"
  tar xvzf linux-$LINUX_KERNEL_VERSION.$CAV_KERNEL_VERSION.tar.gz >> $LOG_FILE_NAME

  cd linux-$LINUX_KERNEL_VERSION.$CAV_KERNEL_VERSION.configured

  msgout "Doing make all"
  make all >> $LOG_FILE_NAME 2>&1
  msgout "Doing make modules_install"
  make modules_install >> $LOG_FILE_NAME 2>&1
  msgout "Doing make install"
  make install >> $LOG_FILE_NAME 2>&1

  #now make this kernel as default in grub.conf
  msgout "Making Cavisson Kernel default for boot"

  GRUB_FILE=/boot/grub/grub.conf
  GRUB_SAVE_FILE=/boot/grub/grub.conf.$$
  GRUB_TMP_FILE=/tmp/grub.conf.$$

  cp $GRUB_FILE $GRUB_SAVE_FILE

  sed "/default=/c\default=0" $GRUB_FILE > $GRUB_TMP_FILE

  IS_FC4=`echo $KERNEL_NAME | grep -c FC4`
 
  #only for FC4 kernel we need to change the grub.conf
  if [ $IS_FC4 == 1 ];then
    #now make this kernel to start without X by replacing "rhgb quiet" by "3 acpi=off"
    msgout "Changing grub.conf for this kernel to start without X by replacing 'rhgb quiet' by '3 acpi=off'"
    sed "/${CAV_KERNEL_VERSION}/s/rhgb quiet/3 acpi=off/" $GRUB_TMP_FILE > $GRUB_FILE
  else
    cp $GRUB_TMP_FILE $GRUB_FILE
  fi

  cd $IDIR
  msgout "Cavisson Kernel installation done"
}

install_cav_kernel
