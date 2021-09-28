if [[ $# -ne 5 ]]; then
echo "Incomplete arguments. $0 <blade> <release> <version> <Machine IP> <portal ip>"
exit 1
fi
WDIR=/home/cavisson/$1
RELEASE=$2
VERSION=$3
MAC_IP=$4
portal_ip=$5
THIRDPARTY=thirdparty.${RELEASE}.${VERSION}_Ubuntu1604_64.bin
THIRDPARTY2=thirdparty.${RELEASE}.${VERSION}.Ubuntu1604_64.bin
NETSTORM=netstorm_all.${RELEASE}.${VERSION}.Ubuntu1604_64.bin
NETSTORM2=netstorm_all.${RELEASE}.${VERSION}_Ubuntu1604_64.bin
. ~/.bashrc
export CALLING_FROM_UPGRADE_SHELL=YES
if [ -f /home/cavisson/bin/nsi_controller_selection_profile ];then
  . /home/cavisson/bin/nsi_controller_selection_profile $CONTROLLER_NAME
else
  . $WDIR/bin/nsi_controller_selection_profile $CONTROLLER_NAME
fi
bold=$(tput bold)
normal=$(tput sgr0)
red=$(tput setaf 1)
green=$(tput setaf 2)

rm -f build_hub.list
if ! nsu_server_admin -s $portal_ip -g -G /home/cavisson/etc/build_hub.list 2>/dev/null; then
  echo "Unable to download build hub list file" >&2
  exit 1
fi
if ! mv build_hub.list ~/etc/; then
  echo "Unable to get build hub list"
  exit 1
fi

_hms()
{
    h=`expr $1 / 3600`
    m=`expr $1  % 3600 / 60`
    s=`expr $1 % 60`
    printf "%02d:%02d:%02d\n" $h $m $s
}

choose_server()
{
  for i in `cat /home/cavisson/etc/build_hub.list | grep -v "^#"`
  do
    local ip=$(echo $i | cut -d ":" -f 1)
    p=$(ping -qc1 -W 5 $ip 2>&1 | awk -F'/' 'END{ print (/^rtt/? $6:"FAIL") }')
    if [[ "x$p" != "xFAIL" ]]; then
      echo "$p $i" >> /tmp/hub.temp
    fi
  done
  sort -k 1,1h /tmp/hub.temp > /tmp/hub1.temp
  rm /tmp/hub.temp
}

download_build()
{
  local BUILD_FILE=$1
  local BUILD_FILE2=$2
  for i in `cat /tmp/hub1.temp|sed -e "s/ /|/g"`
  do
    local hub=$(echo $i | awk -F'|' '{print $2}')
    local latency=$(echo $i | awk -F'|' '{print $1}')
    build_url=ftp://$hub/work/logs/HUB/$BUILD_FILE
    build_url2=ftp://$hub/work/logs/HUB/$BUILD_FILE2
    build_url=https://$hub/logs/HUB/$BUILD_FILE
    build_url2=https://$hub/logs/HUB/$BUILD_FILE2
    echo "$MAC_IP | Selected Build hub $hub based on latency $latency. Downloading $BUILD_FILE" >&2
    wget -c --ftp-user=cavisson --ftp-password=cavisson $build_url --no-check-certificate -q --show-progress --progress=bar:force -P $NS_WDIR/.rel
    if [[ $? -ne 0 ]]; then
      wget -c --ftp-user=cavisson --ftp-password=cavisson $build_url2 --no-check-certificate -q --show-progress --progress=bar:force -P $NS_WDIR/.rel
      if [[ $? -ne 0 ]]; then
        echo "Error downloading file from $hub. Continuing to next hub." >&2
        continue
      else
        BUILD_FILE=$BUILD_FILE2
      fi
    fi
    mv $NS_WDIR/.rel/$BUILD_FILE $NS_WDIR/upgrade
    if [[ "$BUILD_FILE" == thirdparty* ]]; then
      DOWNLOAD_THIRDPARTY_FLAG=1
      THIRDPARTY=$BUILD_FILE
    elif [[ "$BUILD_FILE" == netstorm* ]]; then
      DOWNLOAD_NETSTORM_FLAG=1
      NETSTORM=$BUILD_FILE
    fi
    break
  done
}


mandatory_test()
{
  if a=$(nsu_show_netstorm) ; then
    TR=$(echo $a|awk '{print $5}')
    echo "$MAC_IP|$(basename $WDIR)|TestRun $TR already running."
    exit 1
  fi
}

upgrade_build()
{
  local BUILD_FILE=$1
  cd $NS_WDIR/upgrade
  chmod a+x $BUILD_FILE
#  ./$BUILD_FILE </dev/null >/dev/null 2>/dev/null

  if [[ "$BUILD_FILE" == thirdparty* ]]; then
    mv upgrade_thirdparty_$MAC_IP.log upgrade_thirdparty_$MAC_IP.log.prev
    ./$BUILD_FILE > upgrade_thirdparty_$MAC_IP.log 2>&1
    if [[ $? -eq 0 ]]; then
      UPGRADE_THIRDPARTY_FLAG=1
    fi
  elif [[ "$BUILD_FILE" == netstorm* ]]; then
    mv upgrade_netstorm_$MAC_IP.log upgrade_netstorm_$MAC_IP.log.prev
    ./$BUILD_FILE > upgrade_netstorm_$MAC_IP.log 2>&1
    if [[ $? -eq 0 ]]; then
      UPGRADE_NETSTORM_FLAG=1
    fi
  fi
}


mandatory_test
choose_server
download_build $THIRDPARTY $THIRDPARTY2


if [[ "x$DOWNLOAD_THIRDPARTY_FLAG" == "x1" ]]; then
  upgrade_build $THIRDPARTY
  if [[ "x$UPGRADE_THIRDPARTY_FLAG" == "x1" ]] ; then
    download_build $NETSTORM $NETSTORM2
    if [[ "x$DOWNLOAD_NETSTORM_FLAG" == "x1" ]]; then
      upgrade_build $NETSTORM
      if [[ "x$UPGRADE_NETSTORM_FLAG" == "x1" ]]; then
        echo "$MAC_IP|$(basename $WDIR)|Netstorm $RELEASE.$VERSION upgraded successfully"
        exit 0
      else
        echo "$MAC_IP|$(basename $WDIR)|Error in upgrading netstorm $RELEASE.$VERSION"
        exit 1
      fi
    else
      echo "$MAC_IP|$(basename $WDIR)|Error in Downloading netstorm $RELEASE.$VERSION"
      exit 1
    fi
  else
    echo "$MAC_IP|$(basename $WDIR)|Error in upgrading thirdparty $RELEASE.$VERSION"
    exit 1
  fi
else
  echo "$MAC_IP|$(basename $WDIR)|Error Downloading thirdparty $RELEASE.$VERSION"
  exit 1
fi
