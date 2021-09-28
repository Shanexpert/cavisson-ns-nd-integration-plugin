controllers()
{
  psql -X  -A -U postgres -d demo -t -c "select server_name, row_number() over (order by server_name nulls last), server_name,server_ip,blade_name,ubuntu_version,machine_type,status,team,channel,owner,allocation,shared,build_version, bkp_ctrl, bkp_blade, build_upgradation_date,server_type,controller_ip,controller_blade,refresh_at,bandwidth from allocation where team='$TEAM' and channel='$CHANNEL' and machine_type='Controller' or '$CHANNEL' = ANY(shared) order by server_name" | tr ',' '#'
}
allocated()
{
  psql -X  -A -U postgres -d demo -t -c "select server_name, row_number() over (order by server_name nulls last), * from allocation  where team='$TEAM' and channel='$CHANNEL' and machine_type!='Controller' order by server_name"
}
free()
{
  psql -X  -A -U postgres -d demo -t -c "select server_name, row_number() over (order by server_name nulls last) , *  from allocation where allocation='Free' or allocation='Reserved' order by server_name"
}
defaults()
{
  psql -X  -A -U postgres -d demo -t -c "select server_name, row_number() over (order by server_name nulls last), * from allocation  where team='$TEAM' and channel='default' order by server_name"
}

while getopts t:p:CAFD arg
do
    case $arg in
      t) TEAM=$OPTARG;;
      p) CHANNEL=$OPTARG;;
      C) controllers;;
      A) allocated ;;
      F) free;;
      D) defaults;;
    esac
done
