while getopts r:t: arg
do
        case $arg in
          r)field=2
          user=$OPTARG;;
          t)field=3
          user=$OPTARG;;
        esac
done



  if [[ ! -e $NS_WDIR/server_management/conf/guest_user_mapping.cfg ]]; then
    echo "ERROR: guest_user_mapping.cfg file does not exist"
    exit 1
  fi
  if [ -z "$1" ]; then
    echo "ERROR: Mandatory option missing. Usage: $0 <team name>"
    exit 1
  fi

  if url=$(grep -w $user $NS_WDIR/server_management/conf/guest_user_mapping.cfg | cut -d "|" -f $field); then
    if [[ $(echo $url|wc -l) -gt 1 ]]; then
      echo "ERROR: Duplicate entry in guest_user_mapping.cfg"
      exit 1
    fi
    echo "$url"
  else
    echo "masterBlades.jsp?selectServerName=Name&selectTeam=Team&selectChannel=Project&selectStatus=Status&selectMachineType=Machine+Type&selectAllocation=Allocation&selectServerType=Server+Type"
  fi
