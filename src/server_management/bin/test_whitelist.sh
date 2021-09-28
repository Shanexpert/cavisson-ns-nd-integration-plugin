if [[ $# -ne 2 ]]; then
  echo "Usage: $0 <Team> <IP>"
  exit 1
fi
team=$1
ip=$2
whitelist_cfg=$NS_WDIR/server_management/conf/whitelist.cfg
tmp=/tmp/whitelist.$$
if [ ! -e $whitelist_cfg ]; then
  echo "$whitelist_cfg file does not exist"
  exit 1
fi
main_function()
{
    domain=$(echo $i|cut -d "|" -f 2)
    url="http://$domain"
    echo "curl -o /dev/null --silent --head --write-out '%{http_code}\n' $url" > $tmp.$cnt
    timeout 10s nsu_server_admin -s $ip -g -F $tmp.$cnt -D /tmp
    if [[ $? -eq 0 ]]; then
      status=$(nsu_server_admin -s $ip -g -c "sh $tmp.$cnt")
      echo "$domain : $status"
    else
      echo "$domain : Unable to connect"
    fi

}
cnt=0
if ! grep $team $whitelist_cfg > /dev/null; then
 echo "Entry of $team missing from $whitelist_cfg" >&2
 exit 1
fi
for i in $(grep $team $whitelist_cfg ); do
main_function &
let "cnt=$cnt+1"
done
wait
rm /tmp/whitelist.$$.*
