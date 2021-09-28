#!/bin/sh 

#this shell compile & install the kernel for 3 GB split

debug_log()
{
  echo $@ | tee -a $LOG_FILE_3GB_SPLIT
}

IDIR=`pwd`
LOG_FILE_3GB_SPLIT="$IDIR/cav_kernel_install.log"

rm -rf $IDIR/3GB_SPLIT
mkdir $IDIR/3GB_SPLIT

KERNEL_NAME=`ls linux*tar.gz | awk -F".tar.gz" '{print $1}'`
 
debug_log "Preparing $KERNEL_NAME for 3 GB split........."

debug_log "copying $IDIR/$KERNEL_NAME.tar.gz to $IDIR/3GB_SPLIT/"
cp $IDIR/$KERNEL_NAME.tar.gz $IDIR/3GB_SPLIT/
cp $IDIR/install_kernel.sh  $IDIR/3GB_SPLIT/

cd $IDIR/3GB_SPLIT/

debug_log "Extracting tar $KERNEL_NAME.tar.gz"
tar xvzf $KERNEL_NAME.tar.gz >/dev/null

debug_log "Removing tar $KERNEL_NAME.tar.gz"
rm -f $KERNEL_NAME.tar.gz

debug_log "Moving $KERNEL_NAME.configured ${KERNEL_NAME}_3GBSPLIT.configured"
mv $KERNEL_NAME.configured ${KERNEL_NAME}_3GBSPLIT.configured

cd ${KERNEL_NAME}_3GBSPLIT.configured

debug_log "Making config to 3 GB split"
cp working.config-3GB .config

cp Makefile /tmp/

STRING="`echo $KERNEL_NAME | awk -F"." '{print $NF}'`"
NEW_STRING="${STRING}_3GBSPLIT"

eval sed -e 's/$STRING/$NEW_STRING/g' /tmp/Makefile >Makefile

debug_log "make clean in progress ..."
make clean 

debug_log "make oldconfig in progress ..."
make oldconfig >>$LOG_FILE_3GB_SPLIT
cd ..
debug_log "Making tar ${KERNEL_NAME}_3GBSPLIT.tar.gz ..."
tar cvzf ${KERNEL_NAME}_3GBSPLIT.tar.gz ${KERNEL_NAME}_3GBSPLIT.configured >/dev/null
debug_log "tar ${KERNEL_NAME}_3GBSPLIT.tar.gz completed."

rm -rf /tmp/Makefile ${KERNEL_NAME}_3GBSPLIT.configured

$IDIR/3GB_SPLIT/install_kernel.sh

