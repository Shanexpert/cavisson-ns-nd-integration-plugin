if [[ "xx$NS_WDIR" == "xx" ]]; then
  echo "NS_WDIR not set. exiting"
  exit 1
else
  echo "Current working directory is $NS_WDIR"
fi
IP=$1
if [[ "xx$IP" == "xx" ]]; then
  echo "IP not set. Exiting."
  exit 1
fi
ping -c 1 -w 1 $IP > /dev/null 2>&1
if [[ $? -ne 0 ]]; then
  echo "IP not rechable. Exiting"
  exit 1
fi
DATE=$(date '+%F:%H-%M')
tar -czf NCServerManagement.tar.gz $NS_WDIR/webapps/NCServerManagement
if [[ $? -eq 0 ]]; then
  echo -n -e "\nTar of Jsp pages completed.\n\n"
else
  exit 1
fi
tar -czf server_management.tar.gz $NS_WDIR/server_management
if [[ $? -eq 0 ]] || [[ $? -eq 1  ]]; then
  echo -n -e "\nTar of Server Management completed.\n\n"
else
  exit $?
fi
pg_dump demo -U postgres > archive$DATE.sql
if [[ $? -eq 0 ]]; then
  echo -n  -e "\npg_dump completed.\n\n"
else
  exit 1
fi

nsu_server_admin -s $IP -c date
if [[ $? -ne 0 ]]; then
  echo -n -e  "\nCmon not rechable for Ip $IP\n\n"
  exit 1
fi

echo "Sending admin.tar.gz to $IP"
nsu_server_admin -s $IP -g -F NCServerManagement.tar.gz -D /tmp
if [[ $? -eq 0 ]]; then
  echo -n  -e "\nNCServerManagement.tar.gz sent to $IP.\n\n"
else
  exit 1
fi
echo "Sending server_management.tar.gz to $IP"
nsu_server_admin -s $IP -g -F server_management.tar.gz -D /tmp
if [[ $? -eq 0 ]]; then
  echo -n  -e "\nserver_management.tar.gz sent to $IP.\n\n"
else
  exit 1
fi
echo "Sending archive$DATE.sql to $IP"
nsu_server_admin -s $IP -g -F archive$DATE.sql -D /tmp
if [[ $? -eq 0 ]]; then
  echo -n -e  "\narchive$DATE.sql sent to $IP.\n\n"
else
  exit 1
fi
