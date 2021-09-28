psql  -X  -A -U postgres -d demo -t -c "select distinct server_ip from allocation where server_name='$1'"
