#Author:Gyanendra veeru
#This shell is for sending mail alerts for servers except VMs.
#It sends mail if server is off for last 10 minutes.
#After 1st mail. it sends subsequent mails after 6 hours.
#Config file: mailer.conf.
config=$NS_WDIR/server_management/conf/mailer.conf
tmpdir=$NS_WDIR/server_management/tmp
mailerlog=$NS_WDIR/server_management/logs/mailer.log
cleanup()
{
  rm -f $tmpdir/.mailer.pid
}
if [ "x$1" == "xstop" ]; then
  echo "stopping pid $(cat $tmpdir/.mailer.pid)"
  kill -9 $(cat $tmpdir/.mailer.pid) 2>/dev/null
  cleanup
  exit 1
fi
if [ -e $tmpdir/.mailer.pid ]; then
  echo "ping_cc.sh is already running. Check pid $(cat $tmpdir/.mailer.pid)"
  exit 1
fi
validation()
{
  re='^[0-9]+$'
  if [[ -e $config ]]; then
    FIRSTMAIL=`grep -i "firstmail" $config | grep -v "^#" | awk '{print $2}'`
    if [[ ! $FIRSTMAIL =~ $re ]]; then
      echo "$(date)|Error : Argument for firstmail $FIRSTMAIL is not an integer." >> $mailerlog
      sleep 10
      validation
    fi
    NEXTMAIL=`grep -i "nextmail" $config | grep -v "^#" | awk '{print $2}'`
    if [[ ! $NEXTMAIL =~ $re ]]; then
      echo "$(date)|Error : Argument for nextmail $NEXTMAIL is not an integer." >> $mailerlog
      sleep 10
      validation
    fi
    MAILID=`grep -i "mailid" $config | grep -v "^#" | awk '{for (i=2; i<NF; i++) printf $i " "; print $NF}'`
    FROM=`grep -i "from" $config | grep -v "^#" | awk '{for (i=2; i<NF; i++) printf $i " "; print $NF}'`
  else
    echo "$(date)|Error : mailer.conf file does not exists." >> $mailerlog
    sleep 10
    validation
  fi
}

createhtml()
{
  if [ $1 -eq 1 ]
  then
    local IP=`awk '{print $1}' $tmpdir/mail_body_down.temp | sed '$!{:a;N;s/\n/ and /;ta}'`
    local DURATION=`awk '{print $5}' $tmpdir/mail_body_down.temp | sed '$!{:a;N;s/\n/ and /;ta}'`
    echo "Subject: Service $IP interrupted for $DURATION hrs" > $tmpdir/a.temp
  else
    local IP=`awk '{print $1}' $tmpdir/mail_body_up.temp | sed '$!{:a;N;s/\n/ and /;ta}'`
    local DURATION=`awk '{print $5}' $tmpdir/mail_body_up.temp | sed '$!{:a;N;s/\n/ and /;ta}'`
    echo "Subject: Service $IP continued after $DURATION hrs" > $tmpdir/a.temp
  fi

  echo "Content-Type: text/html; charset=\"us-ascii\"" >> $tmpdir/a.temp
  echo "From: $FROM" >> $tmpdir/a.temp
  echo "To: $MAILID" >> $tmpdir/a.temp
  echo '<html>
  <style>
  .zui-table {
      border: solid 1px #DDEEEE;
      border-collapse: collapse;
      border-spacing: 0;
      font: normal 13px Arial, sans-serif;
  }

  .topnav {
    overflow: hidden;' >> $tmpdir/a.temp
  if [ $1 -eq 1 ]
  then
    echo 'background-color: #580A10;' >> $tmpdir/a.temp
  else
    echo 'background-color: #0B5409;' >> $tmpdir/a.temp
  fi
  echo '}

  .topnav a {
    float: left;
    color: #f2f2f2;
    text-align: center;
    padding: 14px 16px;
    text-decoration: none;
    font-size: 17px;
  }
  .zui-table thead th {
      background-color: #DDEFEF;
      border: solid 1px #DDEEEE;
      color: #336B6B;
      padding: 10px;
      text-align: left;
      text-shadow: 1px 1px 1px #fff;
  }
  .zui-table tbody td {
      border: solid 1px #DDEEEE;
      color: #333;
      padding: 10px;
      text-shadow: 1px 1px 1px #fff;
  }
  .zui-table-rounded {
      border: none;
  }
  .zui-table-rounded thead th {
      background-color: #CFAD70;
      border: none;
      text-shadow: 1px 1px 1px #ccc;
      color: #333;
  }
  .zui-table-rounded thead th:first-child {
      border-radius: 10px 0 0 0;
  }
  .zui-table-rounded thead th:last-child {
      border-radius: 0 10px 0 0;
  }
  .zui-table-rounded tbody td {
      border: none;
      border-top: solid 1px #957030;
      background-color: #EED592;
  }
  .zui-table-rounded tbody tr:last-child td:first-child {
      border-radius: 0 0 0 10px;
  }
  .zui-table-rounded tbody tr:last-child td:last-child {
      border-radius: 0 0 10px 0;
  }
  </style>

  <header>
    <div class="topnav">
    <a>' >> $tmpdir/a.temp

    if [[ $1 -eq 1 ]]; then
      echo "Services Interrupted.." >> $tmpdir/a.temp
    else
      echo "Services Continued.." >> $tmpdir/a.temp
    fi

    echo '</div>
    </header>
    &nbsp;

    <table class="zui-table zui-table-rounded" style="position:relative; left: 25%; right: 20% ">
        <thead>
            <tr>' >> $tmpdir/a.temp

    if [[ $1 -eq 1 ]]; then
      echo '<th>Server Name</th>
      <th>Server IP</th>
      <th>Down Date</th>
      <th>Down Time</th>
      <th>Duration</th>' >> $tmpdir/a.temp
    else
      echo '<th>Server Name</th>
      <th>Server IP</th>
      <th>Up Date</th>
      <th>Up Time</th>
      <th>Duration</th>' >> $tmpdir/a.temp
    fi
    echo '        </tr>
        </thead>' >> $tmpdir/a.temp
    if [[ $1 -eq 1 ]]; then
      cat $tmpdir/mail_body_down.temp | awk 'BEGIN { print "<tbody>" }
     { print "<tr><td>" $1 "</td><td>" $2 "</td><td>" $3 "</td><td>" $4 "</td><td>" $5 "</td></tr>" }
     END   { print "</tbody>" }' >> $tmpdir/a.temp
    else
      cat $tmpdir/mail_body_up.temp | awk 'BEGIN { print "<tbody>" }
     { print "<tr><td>" $1 "</td><td>" $2 "</td><td>" $3 "</td><td>" $4 "</td><td>" $5 "</td></tr>" }
     END   { print "</tbody>" }' >> $tmpdir/a.temp
    fi


     echo '</table>' >> $tmpdir/a.temp
     echo '&nbsp' >> $tmpdir/a.temp
     echo '<hr>
       <h4>Server Management Service</h4>
	<p style="font-style: oblique; font-size:12px;">Click <a href = "https://cc-w-ca-fremont-he-250.cavisson.com/domainadmin/admin/logs.jsp?action=ccvp">here</a> for more information. </p>
       </html>' >> $tmpdir/a.temp
}



_hms()
{
    h=`expr $1 / 3600`
    m=`expr $1  % 3600 / 60`
    s=`expr $1 % 60`
    printf "%02d:%02d:%02d\n" $h $m $s
}

echo $$ > $tmpdir/.mailer.pid
trap "cleanup; exit 1" 1 2 3 6 9

while true
do
  rm -f $tmpdir/mail_body_up.temp 2>/dev/null
  rm -f $tmpdir/mail_body_down.temp 2>/dev/null
  validation
  psql -X -A -U postgres -d demo -t << + > $tmpdir/down_servers.temp
    select server_ip, downtime_epoch, downtime, next_mail, server_name from billing_cc where status='f';
+
  if [[ $? -eq 0 ]]; then
    CURR_EPOCH=$(date +%s);
    DOWN_COUNT=`cat $tmpdir/down_servers.temp | wc -l`
    if [[ $DOWN_COUNT -ne 0 ]]; then
      while read VAR
      do
        SERVER_IP=`echo $VAR|cut -d "|" -f 1`
        EPOCH_TIME=`echo $VAR | cut -d "|" -f 2`
        DOWNTIME=`echo $VAR | cut -d "|" -f 3`
        CURR_NEXT_MAIL=`echo $VAR | cut -d "|" -f 4`
        SERVER_NAME=`echo $VAR | cut -d "|" -f 5`
        DIFF=`expr $CURR_EPOCH - $EPOCH_TIME`
        if [[ $DIFF -gt $FIRSTMAIL ]] && [[ "xx$CURR_NEXT_MAIL" == "xx" ]]; then
          NEXT_MAIL=`expr $CURR_EPOCH + $NEXTMAIL`
          echo "$SERVER_NAME $SERVER_IP $DOWNTIME `_hms $DIFF`"  >> $tmpdir/mail_body_down.temp
          createhtml 1
          psql -X -A -U postgres -d demo -t -c "update billing_cc set next_mail='$NEXT_MAIL' where server_ip='$SERVER_IP' and status='f'" >/dev/null
          FLAG=1
        elif [[ $CURR_NEXT_MAIL -lt $CURR_EPOCH ]] && [[ "xx$CURR_NEXT_MAIL" != "xx" ]]; then
          echo "$SERVER_NAME $SERVER_IP $DOWNTIME `_hms $DIFF`" >> $tmpdir/mail_body_down.temp
          NEXT_MAIL=`expr $CURR_EPOCH + $NEXTMAIL`
          createhtml 1
          psql -X -A -U postgres -d demo -t -c "update billing_cc set next_mail='$NEXT_MAIL' where server_ip='$SERVER_IP' and status='f'" >/dev/null
          FLAG=1
        fi
      done < $tmpdir/down_servers.temp
      if [[ $FLAG -eq 1 ]]; then
        sendmail -t < $tmpdir/a.temp
        if [[ $? -ne 0 ]]; then
          echo "$(date) | Unable to send email to $MAILID. Please check sendmail service." >> $mailerlog
          sleep 100
        fi
#	echo "From: $FROM" > server_admin.temp
#	echo "To: $MAILID" >> server_admin.temp
#	head -1 a.temp >> server_admin.temp
#	for i in `awk '{print $2}' mail_body_down.temp`
#	do17964 pts/8    S      0:00 /bin/sh /home/cavisson/work/server_management/bin/ping_VM.sh
cavisson@controller:~/work/server_management$ ncp_services.sh status ping
  PID TTY      STAT   TIME COMMAND
17963 pts/8    S      0:00 /bin/sh /home/cavisson/work/server_management/bin/ping_CC.sh

#		timeout 10s nsu_server_admin -s $i -c uptime >> server_admin.temp 2>>server_admin.temp
#		if [ $? -eq 124 ]
#		then
#			echo "Timeout 10 sec exceeded connecting to cmon" >> server_admin.temp
#		fi
#	done
#	echo -n -e "\n\nPacket loss samples of last 5 hours\n\n" >> server_admin.temp
#        for i in `awk '{print $1}' mail_body_down.temp`
#        do
#		echo -n -e "\n\n$i\n" >> server_admin.temp
#		cat ../monitoring/ping/.$i.pl | tail -100 >> server_admin.temp
#        done
#	sendmail -t < server_admin.temp

        unset FLAG
      fi
    fi
  else
    echo "$(date)|Error: Unable to get data from postgres. Waiting for 10 sec" >> $mailerlog
    sleep 10
    continue
  fi


  psql -X -A -U postgres -d demo -t << + > $tmpdir/up_servers.temp
  select server_ip, uptime, duration, next_mail, server_name from billing_cc where status='t' and next_mail is not null;
+
  if [[  $? -eq 0  ]]; then
    UP_COUNT=`cat $tmpdir/up_servers.temp | wc -l`
    if [[ $UP_COUNT -ne 0 ]]; then
      while read VAR
      do
        SERVER_IP=`echo $VAR|cut -d "|" -f 1`
        UPTIME=`echo $VAR | cut -d "|" -f 2`
        DURATION=`echo $VAR | cut -d "|" -f 3`
        CURR_NEXT_MAIL=`echo $VAR | cut -d "|" -f 4`
        SERVER_NAME=`echo $VAR | cut -d "|" -f 5`
        echo "$SERVER_NAME $SERVER_IP $UPTIME $DURATION" >> $tmpdir/mail_body_up.temp
        createhtml 2
        psql -X -A -U postgres -d demo -t -c "update billing_cc set next_mail = null where server_ip='$SERVER_IP' and next_mail='$CURR_NEXT_MAIL'"
        FLAG=1
      done < $tmpdir/up_servers.temp
      if [[ $FLAG -eq 1 ]]; then
        sendmail  -t < $tmpdir/a.temp
        if [[ $? -ne 0 ]]; then
          echo "$(date)|Unable to send email to $MAILID. Please check sendmail service." >> $mailerlog
          sleep 100
        fi
        unset FLAG
      fi
    fi
  else
    echo "$(date)|Error: Unable to get data from postgres. Waiting for 10 sec" >> $mailerlog
    sleep 10
    continue
  fi
  sleep 60
done
