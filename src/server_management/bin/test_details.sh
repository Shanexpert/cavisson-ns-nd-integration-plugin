IP=$1

#timeout 10s nsu_server_admin -g -s $IP -c "date" > /dev/null
#if [ $? -ne 0 ]
#then
#  echo "1"
#  echo "server is not accessible or cmon is not running on the server"
#  exit
#else
 
timeout 5s nsu_server_admin -g -s $IP -c "nsu_show_all_netstorm" | cut -d "|" -f 1,2,3,4,5,6,7,8,9,21,22,23 > /tmp/file.temp.$$
if [ $? -ne 0 ]
then
  echo "1"
  echo "server is not accessible or cmon is not running on the server"
  exit
else
  sed -i '1d' /tmp/file.temp.$$
  sed -i  '/directory/d' /tmp/file.temp.$$
  echo "0"
  cat /tmp/file.temp.$$
  rm /tmp/file.temp.$$
fi
