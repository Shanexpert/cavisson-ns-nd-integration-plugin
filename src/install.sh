#!/bin/bash

export HOME_DIR="/home/cavisson"
export NS_WDIR="/home/cavisson/work"
DOMAINADMIN_FILE=$HOME_DIR/etc/cav_domain_appliance.conf
DOMAIN_CONTROLLER_FILE=$HOME_DIR/etc/cav_domain_controllers.conf
CONTROLLER_FILE=$HOME_DIR/etc/cav_controller.conf

#Get DISTRO and RELEASE
export DISTRO=$(./nsi_get_linux_release_ex -d)
export RELEASE=$(./nsi_get_linux_release_ex -r)
#some files for Fedora have FC9 or FC14 as part of the name, so keep old variable

. ./nsi_install_utils >> /dev/null 2>&1

msgout ()
{
  echo "$1" >>$CAPTION
  echo "$1"
}

debug_logs()
{
  echo "$1" >>$CAPTION
}

check_userid()
{
  msgout "Checking user id ..."
  USERID=`id -u`
  if [ $USERID -ne 0 ];then
    msgout "You must be root to execute this command. Exiting."
    exit -1
  fi
}

init_kernel_version()
{
  msgout "Initialising kernel version ..."
  min_arr_name=${DISTRO}_MIN_REL
  max_arr_name=${DISTRO}_MAX_REL
  max_range=$(eval echo \${#${min_arr_name}[@]})

  for((i=0; i<$max_range; i++)) ;do
    min=$(eval echo \${$min_arr_name[$i]})
    max=$(eval echo \${$max_arr_name[$i]})
    echo "min $min max $max"
    if [ $RELEASE -lt $min -o $RELEASE -gt $max ];then
     	continue;
    else
    	 break;
    fi
   done
 
    if [ "$i" == $max_range ]; then 
    echo "Error: release ($RELEASE) is not in the supported range for ($min <= release <= $max for $DISTRO"
    exit -1
  fi
}

create_cavmodem_device()
{
  msgout "Creating cavmodem device"
  if [ -c "/dev/cavmodem" ];then
    msgout "cavmodem already  exist"
  else
    mknod /dev/cavmodem c 10 209
    msgout "cavmodem created"
  fi

  chmod 666 /dev/cavmodem
}

create_test_run_and_ts_run_id()
{
  msgout "Creating test_run_id"
  if [ -f "$HOME_DIR/etc/test_run_id" ];then
    msgout "test_run_id already  exist"
  else
    echo 1000 > $HOME_DIR/etc/test_run_id
    msgout "test_run_id created"
  fi

  msgout "Creating test_suite_number"
  if [ -f "$HOME_DIR/etc/test_suite_run_id" ];then
    msgout "test_suite_run_id already exist"
  else
    echo 1000 > $HOME_DIR/etc/test_suite_run_id
    msgout "test_suite_run_id created"
  fi
  chown -R cavisson:cavisson $HOME_DIR/etc 
}

increase_fdsetsize()
{

  NUM=`grep -c Cavisson /etc/security/limits.conf`
  if [ $NUM -eq 0 ];then
    echo "#Added by Cavisson" >>/etc/security/limits.conf
    echo "*               soft    nofile          262144" >>/etc/security/limits.conf
    echo "*               hard    nofile          262144" >>/etc/security/limits.conf
    echo "*               soft    nproc           65536" >>/etc/security/limits.conf
    echo "*               hard    nproc           65536" >>/etc/security/limits.conf
    echo "*               soft    core            unlimited" >>/etc/security/limits.conf
    echo "*               hard    core            unlimited" >>/etc/security/limits.conf
    echo "root            soft    core            unlimited" >>/etc/security/limits.conf
    echo "root            hard    core            unlimited" >>/etc/security/limits.conf
  fi
}

create_cavisson_user()
{
  #create  user  cavisson
  msgout "Creating  user  cavisson"
  USRSTORM=`cat /etc/passwd|grep "^cavisson:"|wc -l`
  if [ $USRSTORM -ne 1 ];then
        echo "going to be creating user cavisson"
        if [ "X$DISTRO" == "XRedhat" ]; then
            useradd cavisson -m -s /bin/bash
            echo 'cavisson:cavisson' | chpasswd
        else
            useradd -m -s /bin/bash cavisson && echo -e "cavisson\ncavisson" | passwd cavisson >> /dev/null 2>&1
            if [ $? -ne 0 ];then
                echo "User cavisson could not be created successfully"
                exit 1
            fi
        fi
  fi

  chown cavisson:cavisson $HOME_DIR
  sed -i "/cavisson*/d" /etc/sudoers >/dev/null 2>&1
  echo "cavisson ALL=(ALL) NOPASSWD: /usr/sbin/service, /sbin/setcap, /home/cavisson/bin/cav_service, /bin/netstat, /sbin/iptables, /sbin/iptables-save, /sbin/ip, /sbin/route, /usr/bin/strace, /usr/bin/gdb, /usr/bin/ltrace, /sbin/ldconfig, /usr/sbin/iotop, /usr/sbin/tcpdump, /sbin/tc, $JAVA_HOME/bin/jmap" >>/etc/sudoers
  echo 'Defaults  env_keep += "NS_WDIR"' >> /etc/sudoers
  
  chmod +x $HOME_DIR/
  #Make directory "work" under "$HOME_DIR"
  msgout "Creating $NS_WDIR and work, upgrade, etc and .rel directories"
  su cavisson -c "mkdir -p $HOME_DIR/etc"
  su cavisson -c "mkdir -p $HOME_DIR/users"
  su cavisson -c "mkdir -p $NS_WDIR/etc"
  su cavisson -c "mkdir -p $NS_WDIR/upgrade"
  su cavisson -c "mkdir -p $NS_WDIR/.rel"
  su cavisson -c "touch $HOME_DIR/.odbc.ini"
  
 #create bash_profile and call bashrc from bash_profile
  echo -e "if [ -f ~/.bashrc ]; then\n\t. ~/.bashrc\nfi\n. $HOME_DIR/bin/nsi_controller_selection_profile" > $HOME_DIR/.bash_profile
  echo -e "if [ -f ~/.bashrc ]; then\n\t. ~/.bashrc\nfi\n. $HOME_DIR/bin/nsi_controller_selection_profile" > /root/.bash_profile
}

enable_logging_parameters_in_postgresql()
{
  if [ -f $POSTGRESQLCONF ];then
    grep "^[[:blank:]]*#$1" $POSTGRESQLCONF >/dev/null 
    if [ $? -eq 0 ];then
      cmd="sed -i -e \"s/^[[:blank:]]*#$1\(.*\)/#$1\1/g\" -e \"/^#$1/ a\\"$1" = $2\t\t\t\t#Added by install.sh\" $POSTGRESQLCONF"
      eval $cmd
    else
      grep "^[[:blank:]]*$1" $POSTGRESQLCONF >/dev/null
      if [ $? -eq 0 ];then
        cmd="sed -i -e \"s/^[[:blank:]]*$1\(.*\)/#$1\1/g\" -e \"/^#$1/ a\\"$1" = $2\t\t\t\t#Added by install.sh\" $POSTGRESQLCONF"
        eval $cmd
      else
        cmd="sed -i -e \"$ a\\"$1" = $2\t\t\t\t#Added by install.sh\" $POSTGRESQLCONF"
        eval $cmd
      fi
    fi
  else
    msgout "$POSTGRESQLCONF configuration file doesn't exist."
  fi
}

start_postgres_on_boot_ubuntu()
{
  msgout "Configuring postgresql on reboot..."
  if [ ! -f /etc/init.d/postgresql ];then 
    msgout "Configuration failed because postgresql not installed."
    return
  fi
  # Enabling error logging in postgresql, as it is not enabled by default in ubuntu.
  # This logging will not add any overhead as it will log errors only
  enable_logging_parameters_in_postgresql "logging_collector" "on"
  enable_logging_parameters_in_postgresql "log_directory" "'pg_log'"
  enable_logging_parameters_in_postgresql "log_filename" "'postgresql-%a.log'"
  enable_logging_parameters_in_postgresql "log_truncate_on_rotation" "on"
  enable_logging_parameters_in_postgresql "log_rotation_age" "1d"
  enable_logging_parameters_in_postgresql "log_rotation_size" "0"
  enable_logging_parameters_in_postgresql "max_connections" "250"
  chmod 755 /etc/init.d/postgresql
  msgout "Deleting postgresql service."
  update-rc.d -f postgresql remove >> /dev/null 2>&1 
  msgout "Adding postgresql service."
  update-rc.d postgresql  defaults >> /dev/null 2>&1
  update-rc.d postgresql enable 2345 >> /dev/null 2>&1

  #Added by Ankur 18/5/2013
  #link for compiling C files in pgsql
  #The header files are hardcoded in percentile.c
  #need to make more generic

  ln -s /usr/include/postgresql/$POSTGRESQL_VERSION /usr/include/pgsql >>$CAPTION 2>&1
}

############# REDHAT postgresql installation ############
install_ns_database_for_redhat()
{
  #Change the user from "root" to "postgres"
  msgout "Setting up postgres test database"
  echo "!/bin/bash" > /etc/init.d/postgresql
  echo "systemctl \$1 postgresql-12" > /etc/init.d/postgresql
  chmod +x /etc/init.d/postgresql
  su - postgres -c "$IDIR/pginstall.sh $IDIR $CAPTION"
  if [ $? -ne 0 ];then
   echo "postgres setup unsuccessful"
   exit 1
  fi
}

install_ns_database()
{
  #Make the directory "/usr/local/pgsql/data
  msgout "Creating postgress test database for cavisson"
  /etc/init.d/postgresql stop >/dev/null 2>&1
  
  # Anil - Do we need to do this as this will remove old DB
  rm -rf  /var/lib/pgsql/data
  rm -rf $HOME_DIR/pgsql/data

  mkdir -p $HOME_DIR/pgsql/data
  mkdir -p /var/lib/pgsql
  #on ubuntu, we re not able to copy db*.gz into this dir when we run as postgres in the
  #pginstall script. So, set the owner to postgres for now
  chown postgres:postgres /var/lib/pgsql
  #change the ownership  from root to postgres
  msgout "Changing the ownership of postgress test database from root to postgres"
  chown -R postgres:postgres $HOME_DIR/pgsql
  if [ $? -ne 0 ];then
    echo "Could not change ownership"
    exit 1
  fi

  #making the link /var/lib/pgsql/data to $HOME_DIR/pgsql/data
  ln -s $HOME_DIR/pgsql/data /var/lib/pgsql/data  
  if [ $? -ne 0 ];then
    echo "Could not link /var/lib/pgsql/data to $HOME_DIR/pgsql/data"
    exit 1
  fi

  #initdb
  if [[ $RELEASE -eq 1604 ]];then
    POSTGRESQL_VERSION=9.5
  else
    POSTGRESQL_VERSION=12
  fi
  POSTGRES_PATH="/usr/lib/postgresql/$POSTGRESQL_VERSION/bin/"
  msgout "Initializing postgres test database"
  #To  create or initialize the database  cluster
  su postgres -c "$POSTGRES_PATH/initdb -D /var/lib/pgsql/data >>$CAPTION 2>&1"
  if [ $? -ne 0 ];then
    msgout "Could not init postgres"
    exit 1
  fi
  sed  -i -e 's&var\/lib\/postgresql\/'$POSTGRESQL_VERSION'\/main&var\/lib\/pgsql\/data&g' -e '/^ssl\s*=\s*true/ s/^/#/' /etc/postgresql/$POSTGRESQL_VERSION/main/postgresql.conf 
  /etc/init.d/postgresql start >/dev/null 2>&1
  
  #BUG ID : 93306   
  #doc PATH :\cavisson\docs\Products\WebDashboard\GenericDBMonitoring\Req\PostgresMonitoringSetupGuide.docx
  #PURPOSE: changes for enabling postgres monitoring 
  tune_postgres >>/dev/null 2>&1

  #Change the user from "root" to "postgres"
  msgout "Setting up postgres test database"
  su - postgres -c "$IDIR/pginstall.sh $IDIR $CAPTION"
  if [ $? -ne 0 ];then
   echo "postgres setup unsuccessful"
   exit 1
  fi

  start_postgres_on_boot_ubuntu

}

comment_snmp_public_community(){

  sed -i "/^com2sec.*.public.*/s/^/# /" /etc/snmp/snmpd.conf
  sed -i "/rocommunity.*.public.*/s/^/# /" /etc/snmp/snmpd.conf
}

configure_snmp_agent()
{
  msgout "Configuring SNMP agent"

  #Check if SNMP agent is installed or not
  if [ ! -f /etc/init.d/snmpd ]; then
    msgout "Warning: SNMP agent rpms are not installed"
    return
  fi

  NUM=`grep -c "cavissonUser" /etc/snmp/snmpd.conf`
  if [ $NUM -ne 0 ];then
    msgout "SNMP agent is already configured"
    return
  fi

  if [ ! -f snmpd.conf.changes ]; then
    msgout "Warning: SNMP agent conf file is not present in the build"
    return
  fi

  msgout "Stopping SNMP agent"
  /etc/init.d/snmpd stop >/dev/null 2>&1

  msgout "Configuring /etc/snmp/snmpd.conf"
  
  comment_snmp_public_community

  cat snmpd.conf.changes >> /etc/snmp/snmpd.conf

  msgout "Starting SNMP agent"
  /etc/init.d/snmpd start >/dev/null 2>&1

  msgout "Setting SNMP agent for auto start on reboot"

}

add_delete_services()
{
  msgout "Deleting sendmail service..."
  cp -f $IDIR/cavisson /etc/init.d
  chmod +x /etc/init.d/cavisson
  msgout "Adding cavisson service"
  update-rc.d cavisson  defaults >>/dev/null 2>&1
}


install_ssl_file()
{
  su cavisson -c "cp -f $IDIR/cav_ssl* $HOME_DIR/etc/"
  chmod 666 $HOME_DIR/etc/cav_ssl*
}

install_additional_pkgs_ubuntu()
{
  backup_config_files
  mkdir -p /etc/docker
  echo -e "{\n\"graph\":\"/home/cavisson/docker\"\n}\n" > /etc/docker/daemon.json
  ADDITIONAL_RPM_BIN=additional_rpms.${NS_RELEASE}.bin
  
  msgout "Preparing to Install additional Ubuntu packages. for details see file $ADDITIONAL_RPMS_LOGS"

  echo "START: Date: `date +"%m_%d_%Y_%H_%M"`: --------------------------------" >> $ADDITIONAL_RPMS_LOGS 
  echo "Installing additional rpms bin $ADDITIONAL_RPM_BIN." >> $ADDITIONAL_RPMS_LOGS
  echo "PKG_START - List of all pkgs in the machine before installation of additional pkgs:" >> $ADDITIONAL_RPMS_LOGS 
  dpkg -Ol | awk '{print $2}' |sort >> $ADDITIONAL_RPMS_LOGS
  echo "PKG_END - List of all pkgs in the machine before installation of additional pkgs" >> $ADDITIONAL_RPMS_LOGS 

  if [ "X$ADDITIONAL_RPM_BIN" == "X" ];then
    msgout "additional_rpms.$NS_RELEASE.bin not found hence skipping install !"
		return
  fi

  $IDIR/$ADDITIONAL_RPM_BIN

  echo "PKG_START - List of all RPMs in the machine after installation of additional rpms:" >> $ADDITIONAL_RPMS_LOGS 
  dpkg -Ol | awk '{print $2}' |sort >> $ADDITIONAL_RPMS_LOGS
  echo "PKG_END - List of all RPMs in the machine after installation of additional rpms" >> $ADDITIONAL_RPMS_LOGS
  echo "END. Date: `date +"%m_%d_%Y_%H_%M"`: ---------------------------------------" >> $ADDITIONAL_RPMS_LOGS 
  
  msgout "Disable apache2 as it is not required"
  service apache2 stop >>$CAPTION 2>&1
  update-rc.d apache2 disable >> $CAPTION 2>&1

  msgout "Disable bind9 as it is not required"
  service bind9 stop >>$CAPTION 2>&1
  update-rc.d bind9 disable >> $CAPTION 2>&1

  msgout "Disable avahi-daemon as it is not required"
  systemctl stop avahi-daemon.socket >>$CAPTION 2>&1 
  systemctl disable avahi-daemon.socket >>$CAPTION 2>&1
  sed -i s/use-ipv4=yes/use-ipv4=no/g /etc/avahi/avahi-daemon.conf >/dev/null 2>&1
  sed -i s/use-ipv6=yes/use-ipv4=no/g /etc/avahi/avahi-daemon.conf >/dev/null 2>&1
  service avahi-daemon stop >>$CAPTION 2>&1
  update-rc.d avahi-daemon disable >> $CAPTION 2>&1

  msgout "Disable SMBD"
  service smbd stop >>$CAPTION 2>&1
  update-rc.d smbd disable >> $CAPTION 2>&1

  restore_config_files

  msgout "Bind dnsmasq to 127.0.0.1"
  grep "interface=lo" /etc/dnsmasq.conf >>$CAPTION 2>&1
  if [[ -$? == 1 ]];then
  echo "interface=lo" >> /etc/dnsmasq.conf >>$CAPTION 2>&1
  fi
  grep "bind-interfaces" /etc/dnsmasq.conf >>$CAPTION 2>&1
  if [[ -$? == 1 ]];then  
  echo "bind-interfaces" >> /etc/dnsmasq.conf
  fi
  service dnsmasq restart >>$CAPTION 2>&1

}

function get_input()
{
  input=""
  while [ "${input}x" == "x" ]
  do
    read input
  done
  echo $input
}

get_product_config_data()
{
  echo  -n "Enter the $PRODUCT Admin Interface: "
  NAIF=`get_input`
  echo -n "Enter the $PRODUCT Admin IP (Enter in IP/Netbits format): "
  NAIP=`get_input`
  echo -n "Enter the $PRODUCT Admin Gateway to reach NetOcean Admin IP (Enter -, if none): "
  NAGW=`get_input`
  echo -n "Enter the $PRODUCT Load Interface(s), if multiple enter as eth1|eth2 : "
  NLIF=`get_input`
  if [ "$PRODUCT" = "NetStorm" -o "$PRODUCT" = "NetStorm+NetOcean" ]; then
    echo -n "Enter the NetOcean Admin Interface : "
    SAIF=`get_input`
    echo -n "Enter the NetOcean Admin IP (Enter in IP/Netbits format): "
    SAIP=`get_input`
    echo -n "Enter the NetOcean Admin Gateway to reach NetStorm Admin IP (Enter -, if none): "
    SAGW=`get_input`
    echo -n "Enter the Netocean Load Interface(s), if multiple enter as eth1|eth2 : "
    SLIF=`get_input`
  fi
}

function configure_cav_conf(){

if [ -f $CAV_INSTALL -a ! -z $PRODUCT ];then

    system_ip=$(hostname -I | awk '{print $1}')
    BITS=$(ip addr | awk /${system_ip}/ | awk -F '/' {'print $2'} | awk {'print $1'})
    SYSTEM_IP=$system_ip/$BITS
    GATEWAY="-"
    if [[ $DISTRO == "Ubuntu" &&  $RELEASE == "2004" ]];then
	INTERFACE=$(netstat -ie | grep -B1 "${system_ip}" | head -n1 | awk -F ':' '{print $1}')
    else
        INTERFACE=$(netstat -ie | grep -B1 ":${system_ip} " | head -n1 | awk '{print $1}')
    fi
    LOAD_INTERFACE="eth1|eth2"
 
    CAVCONF=$HOME_DIR/etc/cav.conf
    if [ -f $CAVCONF ];then
        rm $HOME_DIR/etc/cav.conf >> /dev/null 2>&1
    fi
        touch $HOME_DIR/etc/cav.conf
        if [ "X$PRODUCT" == "XNS" ];then
            PRODUCTT="NS>NO"
        else
            PRODUCTT=$PRODUCT
        fi
        echo "CONFIG $PRODUCTT" >>$CAVCONF
        echo "NSAdminIF $INTERFACE" >>$CAVCONF
        echo "NSAdminIP $SYSTEM_IP" >>$CAVCONF
        echo "NSAdminGW $GATEWAY" >>$CAVCONF
        echo "NSLoadIF $LOAD_INTERFACE" >>$CAVCONF
        echo "" >>$CAVCONF
    if [ "X$PRODUCT" ==  "XNS" -o "X$PRODUCT" == "XNS+NO" ]; then
        echo "SRAdminIF $INTERFACE" >>$CAVCONF
        echo "SRAdminIP $SYSTEM_IP" >>$CAVCONF
        echo "SRAdminGW $GATEWAY" >>$CAVCONF
        echo "SRLoadIF $LOAD_INTERFACE" >>$CAVCONF
    else
        echo "SRAdminIF $INTERFACE" >>$CAVCONF
        echo "SRAdminIP $SYSTEM_IP" >>$CAVCONF
        echo "SRAdminGW $GATEWAY" >>$CAVCONF
        echo "SRLoadIF $LOAD_INTERFACE" >>$CAVCONF
    fi
    echo "cav.conf file configuration completed"
    APPLIANCE_NAME=$PRODUCT

    echo "adding entries to etc hosts file"
    NUM=`grep -c "NS" /etc/hosts`
    if [ $NUM -eq 0 ];then
        echo "`echo $SYSTEM_IP | cut -d'/' -f1` NS" >> /etc/hosts
        echo "`echo $SYSTEM_IP | cut -d'/' -f1` NO" >> /etc/hosts
    fi

else

  echo -n > $CAVCONF
  echo "Enter System Configuration" 
  echo -n "Enter 1 for NetStorm and 2 for NetOcean and 3 for NetVision and 4 for NetStorm + NetOcean and 5 for NetCloud and 6 for NetDiagnostics 7 for NetChannel: "
  read ANS
  while true
  do
    if [ $ANS -eq 1 ];then
      CAVCONFIG="NS>NO"
      APPLIANCE_NAME="NS"
      PRODUCT="NetStorm"
      break
    elif [ $ANS -eq 2 ];then
      CAVCONFIG="NO"
      APPLIANCE_NAME="NO"
      PRODUCT="NetOcean"
      break
    elif [ $ANS -eq 3 ];then
      CAVCONFIG="NV"
      APPLIANCE_NAME="NV"
      PRODUCT="NetVision"
      break
    elif [ $ANS -eq 4 ];then
      CAVCONFIG="NS+NO"
      APPLIANCE_NAME="NS+NO"
      PRODUCT="NetStorm+NetOcean"
      break
    elif [ $ANS -eq 5 ];then
      CAVCONFIG="NC"
      APPLIANCE_NAME="NC"
      PRODUCT="NetCloud"
      break
    elif [ $ANS -eq 6 ];then
      CAVCONFIG="NDE"
      APPLIANCE_NAME="NDE"
      PRODUCT="NetDiagnostics"
      break
    elif [ $ANS -eq 7 ];then
      CAVCONFIG="NCH"
      APPLIANCE_NAME="NCH"
      PRODUCT="NetChannel"
      break
    else
      echo -n "Enter 1 for NetStorm and 2 for NetOcean and 3 for NetVision and 4 for NetStorm + NetOcean and 5 for NetCloud and 6 for NetDiagnostics 7 for NetChannel: "
      read ANS
    fi
  done
  get_product_config_data $PRODUCT

    echo "CONFIG $CAVCONFIG" >>$CAVCONF
    echo "NSAdminIF $NAIF" >>$CAVCONF
    echo "NSAdminIP $NAIP" >>$CAVCONF
    echo "NSAdminGW $NAGW" >>$CAVCONF
    echo "NSLoadIF $NLIF" >>$CAVCONF
    echo "" >>$CAVCONF

  if [ $ANS -eq 1 -o $ANS -eq 4 ]; then
    echo "SRAdminIF $SAIF" >>$CAVCONF
    echo "SRAdminIP $SAIP" >>$CAVCONF
    echo "SRAdminGW $SAGW" >>$CAVCONF
    echo "SRLoadIF $SLIF" >>$CAVCONF
  else
    echo "SRAdminIF $NAIF" >>$CAVCONF
    echo "SRAdminIP $NAIP" >>$CAVCONF
    echo "SRAdminGW $NAGW" >>$CAVCONF
    echo "SRLoadIF $NLIF" >>$CAVCONF
  fi

fi

}

create_cav_conf()
{
  #Ask Config
  msgout "Configuring $CAVCONF"

  if [ -f $CAVCONF ];then
     rm $HOME_DIR/etc/cav.conf >> /dev/null 2>&1
  fi
  touch $HOME_DIR/etc/cav.conf    
  configure_cav_conf
  chown cavisson:cavisson $CAVCONF
}

create_domain_admin_file()
{
  IP_PORT=`echo $NAIP|cut -d '/' -f1`
  IP_NAME=`echo $NAIP| cut -d '/' -f1|cut -d'.' -f4`
  msgout "Creating domain admin file" 
  if [ ! -f $DOMAINADMIN_FILE ]; then 
    echo "ApplianceName|AdminIP|AdminPort|UI_IP_range|UI_Port_range|ServiceEndpoint_IP_range|ServiceEndpoint_port_range|TomcatOtherPorts|JavaServerPorts|JavaClientPorts|RecorderPorts" >$DOMAINADMIN_FILE
    echo "$APPLIANCE_NAME$IP_NAME|$IP_PORT|7890|$IP_PORT|8001-8400|$IP_PORT|9001-12000|8201-8400|7001-7200|7201-7400|12001-12999" >>$DOMAINADMIN_FILE
  fi 
  msgout "Creating domain controller_file"
  if [ ! -f $DOMAIN_CONTROLLER_FILE ]; then
     echo "ControllerId|ControllerName|owner|password|ApplianceName|UI_IP|UI_Port|ServiceEndPoint_IP|ServiceEndPoint_Port|future1|future2|future3|hpd_IP_ports" > $DOMAIN_CONTROLLER_FILE
  fi
  msgout "Creating controller file"
  if [ ! -f $CONTROLLER_FILE ]; then
    echo "NAME|STATUS|NS_WDIR|HPD_ROOT|TOMCAT_DIR|IpPorts" > $CONTROLLER_FILE
    echo "work|ACTIVE|$NS_WDIR|$HPD_ROOT|$TOMCAT_DIR|NA" >> $CONTROLLER_FILE
  fi
  chown cavisson:cavisson $DOMAINADMIN_FILE
  chown cavisson:cavisson $DOMAIN_CONTROLLER_FILE
  chown cavisson:cavisson $CONTROLLER_FILE
}

update_gc_policy()
{
  #Xmx --  15% of total memory
  #NewSize -- 5% of Xmx
  #PermSize -- 2% of Xmx
  echo "update_gc_policy function has called"
  
  mem_size=`cat /proc/meminfo | awk -F'MemTotal:' '{printf $2}' | sed -e 's/kB//g'`
  mem_size_in_gb=$(($mem_size/1024/1024))
  mem_size_in_mb=$(($mem_size/1024))
  
  Xmx="1g"
  NewSize="500m"
  PermSize="500m"
  
  Xmx_mb="$(($mem_size_in_mb*15/100))"
  Xmx_gb="$(($mem_size_in_gb*15/100))"
  if [ $Xmx_mb -ge 3072 ];then
    Xmx="$(($Xmx_mb/1024))"g
  fi
  
  NewSize_mb="$(($Xmx_mb*5/100))"
  NewSize_gb="$(($Xmx_gb*5/100))"
  if [ $Xmx_mb -gt 3072 -a $NewSize_mb -lt 1024 -a $NewSize_mb -ge 500 ];then
    NewSize="$NewSize_mb"m
  elif [ $NewSize_mb -ge 1024 ];then
    NewSize="$(($NewSize_mb/1024))"g
  fi
  
  PermSize_mb="$(($Xmx_mb*2/100))"
  PermSize_gb="$(($Xmx_gb*2/100))"
  if [ $Xmx_mb -gt 3072 -a $PermSize_mb -lt 1024 -a $PermSize_mb -ge 500 ];then
    PermSize="$PermSize_mb"m
  elif [ $PermSize_mb -ge 1024 ];then
    PermSize="$(($PermSize_mb/1024))"g
  fi
 
  sed -i -e "/^[^#].*TOMCAT_OPTS/d" /home/cavisson/work/sys/site.env >> /dev/null 2>&1
  echo "export TOMCAT_OPTS=\"-XX:NewSize=$NewSize -Xms$Xmx -Xmx$Xmx -XX:ParallelGCThreads=40 -XX:G1HeapRegionSize=32 -XX:+UseG1GC -XX:+PrintCommandLineFlags -XX:+UseStringDeduplication -XX:+PrintGCDetails -XX:+PrintGCTimeStamps -XX:+UnlockCommercialFeatures -XX:+FlightRecorder -Xloggc:\$TOMCAT_DIR/logs/tomcat_gc_\$(date +%Y%m%d%H%M%S).log -XX:+HeapDumpOnOutOfMemoryError -XX:HeapDumpPath=\$NS_WDIR/HeapDump_\$(date +%Y%m%d%H%M%S).hprof -XX:MaxGCPauseMillis=500 -Djava.security.egd=file:/dev/./urandom\"" >> /home/cavisson/work/sys/site.env
}

change_xml_files()
{

  echo "xml function initialized"
  export TOMCAT_VERSION=apache-tomcat-9.0.50
  CTRL_NAME=$2
  TOMCAT_NEW_DIR=$1
  TOMCAT_CONF=$TOMCAT_NEW_DIR/conf
  SERVER_XML=$TOMCAT_CONF/server.xml
  CAVTOMDIR=$IDIR/default_tomcat_files
  if [ "X$MCONFIG" = "XNO" ];then
    SERVER_XML_FILE=serverNO.xml
  else
    SERVER_XML_FILE=server.xml
  fi
  VERSION=`echo $TOMCAT_VERSION|awk -F"-" '{print $3}'`

  if [ -d $CTRL_DIR/apps/$TOMCAT_VERSION ];then
    grep appBase $SERVER_XML | grep -E "/home/cavisson|/home/netstorm"
    if [ $? -ne 0 ]; then
      #configure_server_xml
      mv $TOMCAT_CONF/server.xml $TOMCAT_CONF/server.xml.bak;
      cp $CAVTOMDIR/$SERVER_XML_FILE $TOMCAT_CONF/server.xml;
      #configure_web_xml
      mv $TOMCAT_CONF/web.xml $TOMCAT_CONF/web.xml.bak;
      cp $CAVTOMDIR/web.xml $TOMCAT_CONF/web.xml;
    fi
  fi
  
  sed -i "0,/<Connector port=/s/<Connector port=\"[0-9]*\"/<Connector port=\"$OLD_CONNECTOR_PORT\"/" $SERVER_XML
  sed -i "0,/<Connector SSLEnabled=\"true\" port=/s/<Connector SSLEnabled=\"true\" port=\"[0-9]*\"/<Connector SSLEnabled=\"true\" port=\"$OLD_CONNECTOR_SSL\"/" $SERVER_XML

  sed -i "s/redirectPort=\"443\"/redirectPort=\"$OLD_CONNECTOR_SSL\"/g" $SERVER_XML
  sed -i "s/\<Server port=\"8201\"/Server port=\"$OLD_SERVER_PORT\"/g" $SERVER_XML
  sed -i "s/\<Host name=\"localhost\"  appBase=\"\/home\/cavisson\/work\/webapps\"/Host name=\"localhost\"  appBase=\"\/home\/cavisson\/$CTRL_NAME\/webapps\"/g" $SERVER_XML

  touch "$CTRL_DIR/webapps/404.html"
  grep "error-code" $TOMCAT_CONF/web.xml | grep 404
  if [ $? -ne 0 ];then
    awk '$0 == "</web-app>" {print "   <error-page>\n     <error-code>404</error-code>\n     <location>/404.html</location>\n   </error-page>\n" RS $0 ;next} 1' $TOMCAT_CONF/conf/web.xml > $TOMCAT_CONF/web.xml.$$
    mv $TOMCAT_CONF/web.xml.$$ $TOMCAT_CONF/web.xml
    chmod +x $TOMCAT_CONF/web.xml
  fi

  chmod +r $TOMCAT_CONF/server.xml
  mv -f $TOMCAT_NEW_DIR/conf/context.xml $TOMCAT_NEW_DIR/conf/context.xml.bak;
  if [ -d $CTRL_DIR/apps/$TOMCAT_VERSION ];then
    cp $CAVTOMDIR/context.xml $TOMCAT_CONF/.;
  fi

  mv -f $TOMCAT_NEW_DIR/bin/catalina.sh $TOMCAT_NEW_DIR/bin/catalina.sh.bak;
  if [ -d $CTRL_DIR/apps/$TOMCAT_VERSION ];then
    cp $CAVTOMDIR/catalina.sh $TOMCAT_NEW_DIR/bin/;
  fi
}


install_java_and_tomcat()
{
  msgout "----------------------------------------------------------"  
 
  mkdir -p $HOME_DIR/apps
 
  if [ -d $JAVA_DIR ];then
    msgout "Java is already installed on this system, removing old dir $JAVA_DIR"
    rm -rf $JAVA_DIR
  fi
  cd $HOME_DIR/apps
  #To make sure it is correct directory  "$HOME_DIR/apps"
  PW=`pwd`
  if [ "$PW" != "$HOME_DIR/apps" ];then
    msgout "Unable to change directory to $HOME_DIR/apps "
    exit 1
  fi
  msgout "Installing $JAVA_TAR_NAME"
  cp $IDIR/$JAVA_TAR_NAME .
  if [ $? -ne 0 ];then
    msgout "Unable to copy $JAVA_TAR_NAME"
    exit 1
  fi
  tar xvzf $JAVA_TAR_NAME >>$CAPTION 2>&1  
  if [ $? -ne 0 ];then
    msgout "Error: Error in uncompression of $JAVA_TAR_NAME"
    exit 1
  fi

  if [ "$DISTRO" == "Ubuntu" ]; then
    cp $JAVA_DIR/lib/amd64/jli/libjli.so /usr/lib/x86_64-linux-gnu
  elif [ "$DISTRO" == "Redhat" ]; then
    cp $JAVA_DIR/lib/amd64/jli/libjli.so /usr/lib64
  fi

  mkdir -p $NS_WDIR/apps

  if [ -d $TOMCAT_DIR ];then
    msgout "Tomcat is already installed, removing old dir $TOMCAT_DIR"
    rm -rf $TOMCAT_DIR 
  fi

  cd $NS_WDIR/apps
  #To make sure it is correct directory  "$HOME_DIR/apps"
  PW=`pwd`
  if [ "$PW" != "$NS_WDIR/apps" ];then
    msgout "Unable to change directory to $NS_WDIR/apps "
    exit 1
  fi

  msgout "Installing Apache $TOMCAT_TAR_NAME"
  cp $IDIR/$TOMCAT_TAR_NAME .
  if [ $? -ne 0 ];then
    msgout "Unable to copy $TOMCAT_TAR_NAME"
    exit 1
  fi

  tar xvzf $TOMCAT_TAR_NAME >>$CAPTION 2>&1
  if [ $? -ne 0 ];then
    msgout "Error: Error in uncompression of $TOMCAT_TAR_NAME"
    exit 1
  fi
 
  # Source cavisson.env to set environemnt variables which are based on JDK and Tomcat version
  . $IDIR/netstorm.env
  cd $IDIR  

  chown -R cavisson:cavisson $HOME_DIR/apps
  chown -R cavisson:cavisson $NS_WDIR/apps
}
 
install_node_js()
{
  NODEJS_TAR_NAME=nodejs.tar.gz
  mkdir -p $HOME_DIR/apps

  if [ -d $NODEJS_DIR ];then
    msgout "NODEJS is already installed on this system, removing old dir $NODEJS_DIR"
    rm -rf $NODEJS_DIR
  fi

  cd $HOME_DIR/apps
  #To make sure it is correct directory  "$HOME_DIR/apps"
  PW=`pwd`
  if [ "$PW" != "$HOME_DIR/apps" ];then
    msgout "Unable to change directory to $HOME_DIR/apps "
    exit 1
  fi
  msgout "Installing $NODEJS_TAR_NAME"
  cp $IDIR/$NODEJS_TAR_NAME .
  tar -xvzf $NODEJS_TAR_NAME -C $HOME_DIR/apps/ >>$CAPTION 2>&1
  if [ $? -ne 0 ];then
    msgout "Error: Error in uncompression of $NODEJS_TAR_NAME"
    exit 1
  fi
  ln -s $NODEJS_DIR/bin/npm /usr/bin/ >>$CAPTION 2>&1
  ln -s $NODEJS_DIR/bin/node /usr/bin/ >>$CAPTION 2>&1
  ln -s $NODEJS_DIR/lib/node_modules/pm2/bin/pm2 /usr/bin/ >>$CAPTION 2>&1
 
  cd $IDIR
  chown -R cavisson:cavisson $HOME_DIR/apps

}

save_ns_sys_files()
{
  msgout "Saving Files - $SYS_FILES"
  for file in $SYS_FILES
  do
    if [ -f $NS_WDIR/sys/$file ]; then
      echo "Saving $NS_WDIR/sys/$file as /tmp/$file.$$" >> $CAPTION
      cp $NS_WDIR/sys/$file /tmp/$file.$$
    fi
  done
}

install_thirdparty_components()
{
  msgout "Upgrading Thirdparty components for details see file $THIRDPARTY_LOGS"
  echo "Start: Date: `date +"%m_%d_%Y_%H_%M"` -----------------------------------------" >> $THIRDPARTY_LOGS

  su cavisson -c "mkdir -p $NS_WDIR/upgrade"
  if [ $? != 0 ];then
    echo "Error : Error in creating $NS_WDIR/upgrade"
    echo "Please make this directory manually and try again"
    exit 1
  fi

  export NS_THIRD_PARTY=`ls thirdparty*.bin`  

  echo "Installing thirdparty bin $NS_THIRD_PARTY" >> $THIRDPARTY_LOGS
  echo "Copying $NS_THIRD_PARTY into $NS_WDIR/upgrade" 
  cp $NS_THIRD_PARTY $NS_WDIR/upgrade
  chown cavisson:cavisson $NS_WDIR/upgrade/$NS_THIRD_PARTY

  cp nsi_get_linux_release_ex $NS_WDIR/upgrade
  chown cavisson:cavisson $NS_WDIR/upgrade/nsi_get_linux_release_ex 

  cd $NS_WDIR/upgrade

  # set ldconfig path
  #echo "/home/cavisson/thirdparty/lib" > /etc/ld.so.conf.d/cavlibs.conf
  
  if [ -f /etc/ld.so.conf.d/cavlibs.conf ]; then
     rm -rf /etc/ld.so.conf.d/cavlibs.conf
  fi
 
  su cavisson -c "./$NS_THIRD_PARTY" >> $THIRDPARTY_LOGS 2>> $CAPTION
  if [ $? -eq 0 ];then
      msgout "Thirdparty installed succesfully"
  else
      msgout "Unable to install Thirdparty, hence exiting"
      exit 1
  fi
  cd $IDIR
}

add_cron_entry_for_cavisson()
{
  PATH_VAR="PATH=$NS_WDIR/bin:$HOME_DIR/bin:/usr/bin:/bin"
  CRON_FILE=$HOME_DIR/.crontab
  CRON_STRING="* * * * * $HOME_DIR/bin/nsu_check_health" #every minute
  echo "Adding cron entry for check controller health instance directories" >> $CAPTION
  echo "$PATH_VAR" > $CRON_FILE
  echo "$CRON_STRING" >>$CRON_FILE
  crontab -u cavisson $CRON_FILE
  rm -f $CRON_FILE >/dev/null 2>&1
}
add_cron_entry_for_root()
{
  PATH_VAR="PATH=$NS_WDIR/bin:$HOME_DIR/bin:/usr/bin:/bin"
  CRON_FILE=$HOME_DIR/.crontab
  CRON_STRING="00 21 * * * $HOME_DIR/bin/nsi_clean_files" #Starting cron daily at 9 PM IST
  echo "Adding cron entry for clean up of instance directories" >> $CAPTION
  echo "$PATH_VAR" >> $CRON_FILE
  echo "$CRON_STRING" >> $CRON_FILE

  CRON_STRING="*/5 * * * * /etc/init.d/scheduler start"
  echo "$CRON_STRING" >> $CRON_FILE

  crontab -u root $CRON_FILE
  #rm -f $CRON_FILE >/dev/null 2>&1
}

create_ns_users()
{
  
  echo "Adding cavisson group" >> $CAPTION
  $NS_WDIR/bin/nsu_add_group -n cavisson -t Engineers -d "cavisson is the Engineers" >> $CAPTION
  echo "Adding guest group" >> $CAPTION
  $NS_WDIR/bin/nsu_add_group -n guest -t Observers -d "guest is the Observers" >> $CAPTION

  echo "Adding cavisson user" >> $CAPTION
  $NS_WDIR/bin/nsu_add_user -u cavisson -g cavisson -p cavisson -e NA -d All/All >> $CAPTION
  echo "Adding guest user" >> $CAPTION
  $NS_WDIR/bin/nsu_add_user -u guest -g guest -p guest -e NA -d All/All >> $CAPTION
  
  chown cavisson:cavisson $HOME_DIR/users
  sudo -u postgres createuser cavisson >/dev/null 2>&1
}

create_ns_services()
{

  echo "going to start all ns related services like tomcat, hpd, cmon, etc."
  cp $NS_WDIR/webapps/scheduler/bin/init_scheduler /etc/init.d/scheduler
  chmod +x /etc/init.d/scheduler
  update-rc.d scheduler defaults >>$CAPTION 2>&1 

  cp $NS_WDIR/webapps/.tomcat/tomcat /etc/init.d/tomcat
  chmod +x /etc/init.d/tomcat
  update-rc.d tomcat defaults >>$CAPTION 2>&1 

  cp $NS_WDIR/hpd/bin/hpd /etc/init.d/hpd
  chmod +x /etc/init.d/hpd
  update-rc.d hpd defaults >>$CAPTION 2>&1 

  cp $NS_WDIR/hpd/bin/rsync_daemon /etc/init.d/rsync_daemon
  chmod +x /etc/init.d/rsync_daemon
  update-rc.d rsync_daemon defaults >>$CAPTION 2>&1 
  
  cp $NS_WDIR/bin/system_health /etc/init.d/system_health 
  chmod +x /etc/init.d/system_health
  update-rc.d system_health defaults >>$CAPTION 2>&1

  cp $NS_WDIR/lps/bin/lps /etc/init.d/lps
  chmod +x /etc/init.d/lps
  update-rc.d lps defaults >>$CAPTION 2>&1

  cp $NS_WDIR/ndc/bin/ndc /etc/init.d/ndc
  chmod +x /etc/init.d/ndc
  update-rc.d ndc defaults >>$CAPTION 2>&1

  cp $NS_WDIR/netchannel/bin/nch /etc/init.d/nch
  chmod +x /etc/init.d/nch
  update-rc.d nch defaults >>$CAPTION 2>&1
  
  cp $HOME_DIR/monitors/bin/cmon /etc/init.d/cmon
  chmod +x /etc/init.d/cmon
  update-rc.d cmon defaults >>$CAPTION 2>&1
  
  cp $NS_WDIR/bin/cav_service $HOME_DIR/bin
  sudo chattr +i  $HOME_DIR/bin/cav_service

  cp $NS_WDIR/statsc/bin/statsc /etc/init.d/statsc
  chmod +x /etc/init.d/statsc
  update-rc.d statsc defaults >>$CAPTION 2>&1

  #API_GATEWAY 
  cp $NS_WDIR/api_gateway/bin/apiGateway /etc/init.d/apiGateway
  chmod +x /etc/init.d/apiGateway
  update-rc.d apiGateway defaults >>$CAPTION 2>&1

  /etc/init.d/tomcat start
  /etc/init.d/hpd start
  /etc/init.d/ndc start
  /etc/init.d/scheduler start
  /etc/init.d/system_health start  
  /etc/init.d/rsync_daemon start
  /etc/init.d/lps start
  /etc/init.d/cmon start
  /etc/init.d/statsc start
  /etc/init.d/nch start
  /etc/init.d/apiGateway start
}

install_ns_components()
{
  msgout "Upgrading NetStorm components for details see file $NS_LOGS"

  su cavisson -c "export NS_WDIR=\"/home/cavisson/work\""
  cp $IDIR/netstorm.env $NS_WDIR/etc/
  chown cavisson:cavisson $NS_WDIR/etc/netstorm.env
  
  save_ns_sys_files
  
  echo "START: Date: `date +"%m_%d_%Y_%H_%M"` : ------------------------------------" >>$NS_LOGS

  # Must set before doing cd
  export NS_BIN_FILE=`ls netstorm_all*bin`
  
  echo "Installing ns bin $NS_BIN_FILE" >>$NS_LOGS
  echo "Copying $NS_BIN_FILE into $NS_WDIR" >>$NS_LOGS

  cp $NS_BIN_FILE $NS_WDIR/upgrade
  cd $NS_WDIR/upgrade

  chown -R cavisson:cavisson $NS_WDIR/upgrade
  chown -R cavisson:cavisson $NS_WDIR/.rel

  su cavisson -c "mkdir -p $NS_WDIR/scenarios/default/default"
  su cavisson -c "mkdir -p $NS_WDIR/scripts/default/default"
  su cavisson -c "mkdir -p $NS_WDIR/checkprofile/default/default"

  msgout "Starting netstorm all component installation"
  if [[ $DISTRO == "Ubuntu" &&  $RELEASE == "2004" ]];then
      mknod -m 644 /dev/tty c 5 0 >>$CAPTION 2>&1
      chmod o+rw /dev/tty >>$CAPTION 2>&1
      su cavisson -c "source /home/cavisson/work/etc/netstorm.env && ./$NS_BIN_FILE" >>$NS_LOGS 2>>$CAPTION
      if [ $? -eq 0 ];then
          msgout "NS_All installed succesfully"
      else
          msgout "Unable to install NS_All, Please install $NS_BIN_FILE manually. hence exiting"
          exit 1
      fi
  else
      su cavisson -c "./$NS_BIN_FILE" >>$NS_LOGS 2>>$CAPTION
      if [ $? -eq 0 ];then
          msgout "NS_All installed succesfully"
      else
          msgout "Unable to install NS_All, Please install $NS_BIN_FILE manually. hence exiting"
          exit 1
      fi
  fi
  echo "END: Date: `date +"%m_%d_%Y_%H_%M"` : ---------------------------------------------" >>$NS_LOGS

  restore_ns_sys_files
  add_cron_entry_for_root
  #add_cron_entry_for_cavisson
  create_ns_users
  update_gc_policy
  cd $IDIR 

}

restore_ns_sys_files()
{
  msgout "Creating/Restoring Files - $SYS_FILES"
  mkdir -p $NS_WDIR/sys
  chmod 777 $NS_WDIR/sys
  for file in $SYS_FILES
  do
    if [ -f /tmp/$file.$$ ]; then
      echo "Restoring $NS_WDIR/sys/$file" >> $CAPTION
      mv /tmp/$file.$$ $NS_WDIR/sys/$file
    else
      echo "Creating $NS_WDIR/sys/$file" >> $CAPTION
      if [ $file == "ip_properties" ]; then
        echo 'USE_FIRST_IP_AS_GATEWAY 0' > $NS_WDIR/sys/$file
        echo 'DUT_LAYER 0' >> $NS_WDIR/sys/$file
        echo 'RESERVED_NETID 192.168.255.0' >> $NS_WDIR/sys/$file
        chmod 666 $NS_WDIR/sys/$file
      elif [ $file == "site.env" ]; then
        cp $IDIR/site.env $NS_WDIR/sys/$file
        chown cavisson:cavisson $NS_WDIR/sys/$file
        chmod 777 $NS_WDIR/sys/$file
      else
        touch $NS_WDIR/sys/$file
        chown cavisson:cavisson $NS_WDIR/sys/$file
        chmod 666 $NS_WDIR/sys/$file
      fi
    fi
  done
  chown -R cavisson:cavisson $NS_WDIR/sys
}

#Extract Eval scripts and scenarios
copy_scripts_scenario()
{
  msgout "Copying evaluation scripts and scenario files"

  cp $NS_WDIR/samples/*.conf $NS_WDIR/workspace/admin/default/cavisson/default/default/scenarios
  chown cavisson:cavisson $NS_WDIR/workspace/admin/default/cavisson/default/default/scenarios/*.conf
  chmod 664 $NS_WDIR/workspace/admin/default/cavisson/default/default/scenarios/*.conf

  cd $NS_WDIR/workspace/admin/default/cavisson/default/default/scripts
  for script in `ls $NS_WDIR/samples/scr_*.tar.gz`
  do
    tar xvzf $script >> $CAPTION 2>&1
    chown -R cavisson:cavisson $NS_WDIR/workspace/admin/default/cavisson/default/default/scripts
  done
}

#Add entries in /etc/hosts to fix the issue of ssh taking lot of time
#Need to discuss how to put hostnames
add_entries_to_etc_host()
{
  msgout "Adding entries in /etc/hosts file"
  NUM=`grep -c "NS" /etc/hosts`
  if [ $NUM -eq 0 ];then
    echo "`echo $NAIP | cut -d'/' -f1` NS" >> /etc/hosts
    echo "`echo $SAIP | cut -d'/' -f1` NO" >> /etc/hosts
  fi
}

get_key_from_file()
{ 
  COMPONENT=$1
  BUILD_FILE_NAME=`egrep "^$COMPONENT=" $INSTALL_SPEC_FILE | awk -F '=' '{print $2}'`
  if [ "X$BUILD_FILE_NAME" == "X" ];then 
    echo "NS_RELEASE does not exist in install.spec file.Hence exiting.."
    exit -1
  fi
}

validate_Cav()
{
  if [ ! -f $INSTALL_SPEC_FILE ];then
    msgout "Error: The $INSTALL_SPEC_FILE must be present for upgrade the NetStorm Components"
    exit 1
  fi
  get_key_from_file "NS_RELEASE"
  if [ $NS_RELEASE != $BUILD_FILE_NAME ];then
    echo "CAVBIN RELEASE did'nt match with machine release, hence exiting"
    exit -1
  fi
}

#this is not complete- as I dont know what we are trying to achieve here ?
disable_firewall_ubuntu(){

IPTABLES_FILE=/etc/iptables
IPTABLES_FILE_ORIG=/etc/iptables.orig
IP6TABLES_FILE=/etc/ip6tables
IP6TABLES_FILE_ORIG=/etc/ip6tables.orig

if [[ $(ufw status) == "Status: inactive" ]];then 
  echo "firewall is inactive"
elif [[ $(ufw status) == "Status: active" ]];then  
  ufw disable
fi

}

#Disable the network manager
disable_network_manager_ubuntu()
{
  msgout "Disable network manager"
  
  cd /etc/network/
  
  if [ ! -f /etc/init.d/network-manager ];then
    msgout "Warrning: Network manager not exist."
    return   
  fi

  stat=`service network-manager status` >>$CAPTION 2>&1 
  if [[ $stat == "network-manager start/running" ]];then
    debug_logs "Network manager is running"
    service network-manager stop >>$CAPTION 2>&1 
  fi

  # always disable
  update-rc.d -f network-manager remove >>$CAPTION 2>&1 
  if [ $? == 0 ];then
      msgout "network managaer disabled successfully."  
  fi
  
  #after disabling from rc.d, it can still be init
  #so comment start option in /etc/init/network-manager.conf
  if [ -f /etc/init/network-manager.conf ];then
    sed -i '/start on/,/)/s/^/#/g' /etc/init/network-manager.conf 1>/dev/null 2>&1
  fi

  #enable networing service
  #chkconfig  -s network 2345 >> $CAPTION 2>&1 
  update-rc.d networking defaults >>$CAPTION 2>&1
  if [ $? == 0 ];then
      msgout "network start successfully."  
  fi
}

#Configure dns 
configure_dns()
{
  msgout "Configuring DNS server"
  name_server=$(cat /etc/resolv.conf | grep "8.8.8.8")
  if [ -z "$name_server" ];then
      echo "nameserver 8.8.8.8" >>/etc/resolv.conf
  fi

}

#Disable Apparmor Service
disable_apparmor_ubuntu()
{
  msgout "Disable Apparmor Daemon"
  
  if [ ! -f /etc/init.d/apparmor ];then
    msgout "Warrning: apparmor init file not found"
    return   
  fi

  #Check if apparmor is running.
  /etc/init.d/apparmor status >>$CAPTION 2>&1
  if [ $? -eq 0 ];then
    msgout "apparmor is running, stoping..."
    sudo systemctl stop apparmor >>$CAPTION 2>&1
  fi 

  # always disable
  sudo systemctl disable apparmor >>$CAPTION 2>&1 
  if [ $? == 0 ];then
      msgout "Apparmor disabled successfully."  
  fi

  #rclocal=`grep "^/etc/init.d/apparmor teardown" /etc/rc.local` >>$CAPTION 2>&1
  #It may enabled by some other services. so teardown in /etc/rc.local
  #if [ -z "$rclocal" ];then 
    #sed -i '/^exit\s*0/i /etc/init.d/apparmor teardown' /etc/rc.local >>$CAPTION 2>&1
  #fi

}

modify_grub_for_text_mode()
{
  if [ ! -f /etc/default/grub ];then
    msgout "Warning: grub configuration file /etc/default/grub does not exist"
    return  
  fi

  #set GRUB_CMDLINE_LINUX_DEFAULT="text"
  sed -i 's/^GRUB_CMDLINE_LINUX_DEFAULT.*$/GRUB_CMDLINE_LINUX_DEFAULT="text"/g' /etc/default/grub
  
  #enable GRUB_HIDDEN_TIMEOUT 
  #if GRUB_HIDDEN_TIMEOUT keyword exist then modify else append
  if [ ! -z "`grep "^GRUB_HIDDEN_TIMEOUT" /etc/default/grub`" ];then 
    sed -i 's/^GRUB_HIDDEN_TIMEOUT.*$/GRUB_HIDDEN_TIMEOUT=10/g' /etc/default/grub
  else
    echo "GRUB_HIDDEN_TIMEOUT=10" >>/etc/default/grub
  fi
 
  #update grub setting.
  update-grub >>$CAPTION 2>&1
  if [ $? -eq 0 ];then
    msgout "Grub setting successfully updated"
  fi
}

#Changes done as in ubuntu machine , at command was not working
copy_at_file_SQ()
{
  if [[ $DISTRO == "Ubuntu" ]] ;then
    msgout "Creating .SEQ file and changing the permisson of atjobs"
    if [[ $RELEASE -eq 1604 || $RELEASE -eq 2004 ]] ;then
      touch /var/spool/cron/crontabs/.SEQ
      cd /var/spool/cron/crontabs
    else 
      touch /var/spool/cron/atjobs/.SEQ
      cd /var/spool/cron/atjobs
    fi
    chown daemon.daemon .SEQ
  fi
}

#default path is taken from /etc/environment for su commands  in Ubuntu
#Note: the su man page says that it takes it from /etc/login.defs, but I found otherwise
#Therefore currently we point FILE_FOR_SU_PATH to /etc/environment
modify_path_for_su()
{
  tmp_file=`echo "$FILE_FOR_SU_PATH" | sed 's/\//_/g;s/$/\.'$$'/'`
#save existing file just in case we mess up if it doesnt already exist- this means we will 
#preserve only the very first time and not the intermediate ones
  if [ -f $FILE_FOR_SU_PATH.sav -a -s $FILE_FOR_SU_PATH.sav ] ;then 
    cp $FILE_FOR_SU_PATH.sav $FILE_FOR_SU_PATH
  else
    cp $FILE_FOR_SU_PATH $FILE_FOR_SU_PATH.sav
  fi
#add this path to the existing path in FILE_FOR_SU_PATH
  ADD_PATH="$NS_WDIR/bin:$NS_WDIR/tools:$JAVA_HOME/bin"
  QUOTED_ADD_PATH="`echo $ADD_PATH | sed 's&\/&\\\/&g'`"
#comment existing PATH line and print that out. modify PATH line and print that out too.
  sed 'h;/^PATH/ s/^/#/;x; s/"$/:'$QUOTED_ADD_PATH'\"/;x;p;x' $FILE_FOR_SU_PATH > /tmp/$tmp_file
  mv /tmp/$tmp_file $FILE_FOR_SU_PATH
}

disable_auto_ip_assign()
{
  if [ -f $3 ];then
    grep "^[[:blank:]]*#$1" $3 >/dev/null
    if [ $? -eq 0 ];then
      cmd="sed -i -e \"s/^[[:blank:]]*#$1\(.*\)/#$1\1/g\" -e \"/^#$1/ a\\"$1"=$2\" $3"
      eval $cmd
    else
      grep "^[[:blank:]]*$1" $3 >/dev/null
      if [ $? -eq 0 ];then
        cmd="sed -i -e \"s/^[[:blank:]]*$1\(.*\)/#$1\1/g\" -e \"/^#$1/ a\\"$1"=$2\" $3"
        eval $cmd
      else
        cmd="sed -i -e \"$ a\\"$1"=$2\" $3"
        eval $cmd
      fi
    fi
  else
    msgout "$3 configuration file doesn't exist."
  fi
}

# setting default timezone to US/Central
set_default_timezone()
{
  rm /etc/localtime
  ln -s /usr/share/zoneinfo/US/Central /etc/localtime
}

start_postgres_clusters()
{
  postgres_pid=`ps -ef | grep postgres | awk 'FNR == 2 {print $2}'`
  kill -9 $postgres_pid
  /etc/init.d/postgresql start > /dev/null 2>&1
}

remove_comments() 
{
  echo "$( cat $1 | sed 's/<!--/\x0<!--/g;s/-->/-->\x0/g' | grep -zv '^<!--' | tr -d '\0' | grep -v "^\s*$")" | tr '\n' ' ' | tr '>' '\n' > /tmp/server.xml.tmp
}

save_ports_value()
{
  CTRL_DIR="/home/cavisson/$1" 
  CTRL_APPS_DIR="$CTRL_DIR/apps"
  if [ -d $CTRL_APPS_DIR/apache-tomcat-9.0.50 ];then
    TOMCAT_OLD_DIR=$CTRL_APPS_DIR/apache-tomcat-9.0.50
  elif [ -d $CTRL_APPS_DIR/apache-tomcat-9.0.43 ];then
    TOMCAT_OLD_DIR=$CTRL_APPS_DIR/apache-tomcat-9.0.43
  elif [ -d $CTRL_APPS_DIR/apache-tomcat-9.0.41 ];then
    TOMCAT_OLD_DIR=$CTRL_APPS_DIR/apache-tomcat-9.0.41
  elif [ -d $CTRL_APPS_DIR/apache-tomcat-7.0.105 ];then
    TOMCAT_OLD_DIR=$CTRL_APPS_DIR/apache-tomcat-7.0.105
  elif [ -d $CTRL_APPS_DIR/apache-tomcat-7.0.104 ];then
    TOMCAT_OLD_DIR=$CTRL_APPS_DIR/apache-tomcat-7.0.104
  elif [ -d $CTRL_APPS_DIR/apache-tomcat-7.0.99 ];then
    TOMCAT_OLD_DIR=$CTRL_APPS_DIR/apache-tomcat-7.0.99
  elif [ -d $CTRL_APPS_DIR/apache-tomcat-7.0.91 ];then
    TOMCAT_OLD_DIR=$CTRL_APPS_DIR/apache-tomcat-7.0.91
  elif [ -d $CTRL_APPS_DIR/apache-tomcat-7.0.59 ];then
    TOMCAT_OLD_DIR=$CTRL_APPS_DIR/apache-tomcat-7.0.59
  elif [ -d $CTRL_APPS_DIR/apps/apache-tomcat-7.0.52 ];then
    TOMCAT_OLD_DIR=$CTRL_APPS_DIR/apache-tomcat-7.0.52
  fi

  remove_comments $TOMCAT_OLD_DIR/conf/server.xml
  OLD_CONNECTOR_PORT=`cat /tmp/server.xml.tmp | grep "<Connector" | grep HTTP | grep -v SSLEnabled | awk -F"port=" {'print $2'} | cut -d'"' -f 2`
  OLD_CONNECTOR_SSL=`cat /tmp/server.xml.tmp | grep "<Connector" | grep HTTP | grep -v SSLEnabled | awk -F"redirectPort=" {'print $2'} | cut -d'"' -f 2`
  OLD_SERVER_PORT=`cat /tmp/server.xml.tmp | grep "<Server" | awk -F"port=" {'print $2'}| cut -d'"' -f 2`

}

install_tomcat()
{
  CONTROLLER_CONFIG_FILE=/home/cavisson/etc/cav_controller.conf
  controller_list=`cat $CONTROLLER_CONFIG_FILE |grep -v '^#' |awk -F'|' '{if(NR!=1)print $1}'`
  i=1
  ctrl_name=`echo $controller_list |awk '{print $1}'`
  while [ "X$ctrl_name" !=  "X" ];do
    ctrl_count=`expr $ctrl_count + 1`
    if [ ! -d /home/cavisson/$ctrl_name/apps/$TOMCAT_VERSION  ];then
      echo "As we are upgrading the tomcat version to $TOMCAT_VERSION  on $ctrl_name, Please change your customized content(if present) of apache directory after upgradation"
      save_ports_value "$ctrl_name"
      cp $IDIR/$TOMCAT_TAR_NAME /home/cavisson/$ctrl_name/apps/
      tar -xvzf /home/cavisson/$ctrl_name/apps/$TOMCAT_TAR_NAME -C /home/cavisson/$ctrl_name/apps/
      chown -R cavisson:cavisson /home/cavisson/$ctrl_name/apps/$TOMCAT_VERSION 
      change_xml_files "/home/cavisson/$ctrl_name/apps/$TOMCAT_VERSION" "$ctrl_name"
    fi
    i=`expr $i + 1`
    ctrl_name=`echo $controller_list |  awk -v "j=$i" '{print $j}'`
  done
}

#backup files
backup_config_files()
{
  #dns file backup
  if [ -f "/var/run/dnsmasq/resolv.conf" ];then
     cp /var/run/dnsmasq/resolv.conf $IDIR/resolv.conf
  fi     
  #sshd file backup
  if [ -f /etc/ssh/sshd_config ]; then
    cp /etc/ssh/sshd_config $IDIR/sshd_config
  fi
}

#restoring_bkup_files
restore_config_files()
{
#dns file restoring
  if [ -f $IDIR/resolv.conf ];then
   mv $IDIR/resolv.conf /var/run/dnsmasq/resolv.conf
  fi
#restoring ssh
  if [ -f $IDIR/sshd_config ]; then
    mv $IDIR/sshd_config /etc/ssh/sshd_config
  fi
}

upgrade_packages()
{
 MCONFIG=`grep CONFIG $HOME_DIR/etc/cav.conf | awk '{print $2}'` 
  if [ $DISTRO == "Ubuntu" ] ;then
    msgout "Adding iptables rules"
    add_iptables_rules
  
    $NS_WDIR/tools/nsu_iptables -L >> $CAPTION 2>&1
    chown cavisson:cavisson $NS_WDIR/etc/iptables/rules.v4

    if [ -f /etc/mongod.conf ];then
        rm /etc/mongod.conf >>$CAPTION 2>&1
    fi
    msgout "Upgrade Packages"
    install_additional_pkgs_ubuntu
    gitlab-runner_configuration
  fi 

  msgout "Upgrade Java"
  mkdir -p $HOME_DIR/apps/
  if [ $? -ne 0 ]; then
    msgout "mkdir $HOME_DIR/apps/ failed"
    exit 1
  fi
  rm -rf $HOME_DIR/apps/jdk*

  msgout "Installing jdk1.8.0_301"
  #Make one directory  called "java" under "/apps"
  #change the directory to "/apps/java"
  cd  $HOME_DIR/apps/

  #To make sure it is right directory  "/apps/java"
  PD=`pwd`
  if [ $PD != "$HOME_DIR/apps" ];then
    msgout "Unable to cd $HOME_DIR/apps/"
    exit 1
  fi

  #Copy "Java Tar"  to current directory(/apps/java)
  cp $IDIR/$JAVA_TAR_NAME .
  if [ $? -ne 0 ];then
    msgout "Unable to copy jdk"
    exit 1
  fi
  #Run "Java bin"
  tar xvzf $JAVA_TAR_NAME >>$CAPTION 2>&1

  if [ $? -ne 0 ];then
    msgout "jdk run is unsuccessful...."
    exit 1
  fi

  # Source cavisson.env to set environemnt variables which are based on JDK and Tomcat version
  cp $IDIR/netstorm.env $NS_WDIR/etc/
  sed -i 's/jdk1.7.0_71/jdk1.8.0_301/g' /etc/environment
  sed -i 's/jdk1.7.0_71/jdk1.8.0_301/g' /etc/bash.bashrc
  sed -i 's/jdk1.8.0_112/jdk1.8.0_301/g' /etc/environment
  sed -i 's/jdk1.8.0_112/jdk1.8.0_301/g' /etc/bash.bashrc
  sed -i 's/jdk1.8.0_121/jdk1.8.0_301/g' /etc/environment
  sed -i 's/jdk1.8.0_121/jdk1.8.0_301/g' /etc/bash.bashrc
  sed -i 's/jdk1.8.0_144/jdk1.8.0_301/g' /etc/environment
  sed -i 's/jdk1.8.0_144/jdk1.8.0_301/g' /etc/bash.bashrc
  sed -i 's/jdk1.8.0_181/jdk1.8.0_301/g' /etc/environment
  sed -i 's/jdk1.8.0_181/jdk1.8.0_301/g' /etc/bash.bashrc
  sed -i 's/jdk1.8.0_191/jdk1.8.0_301/g' /etc/environment
  sed -i 's/jdk1.8.0_191/jdk1.8.0_301/g' /etc/bash.bashrc
  sed -i 's/jdk1.8.0_201/jdk1.8.0_301/g' /etc/environment
  sed -i 's/jdk1.8.0_201/jdk1.8.0_301/g' /etc/bash.bashrc
  sed -i 's/jdk1.8.0_211/jdk1.8.0_301/g' /etc/environment
  sed -i 's/jdk1.8.0_211/jdk1.8.0_301/g' /etc/bash.bashrc
  sed -i 's/jdk1.8.0_221/jdk1.8.0_301/g' /etc/environment
  sed -i 's/jdk1.8.0_221/jdk1.8.0_301/g' /etc/bash.bashrc
  sed -i 's/jdk1.8.0_231/jdk1.8.0_301/g' /etc/environment
  sed -i 's/jdk1.8.0_231/jdk1.8.0_301/g' /etc/bash.bashrc
  sed -i 's/jdk1.8.0_241/jdk1.8.0_301/g' /etc/environment
  sed -i 's/jdk1.8.0_241/jdk1.8.0_301/g' /etc/bash.bashrc
  sed -i 's/jdk1.8.0_251/jdk1.8.0_301/g' /etc/environment
  sed -i 's/jdk1.8.0_251/jdk1.8.0_301/g' /etc/bash.bashrc
  sed -i 's/jdk1.8.0_261/jdk1.8.0_301/g' /etc/environment
  sed -i 's/jdk1.8.0_261/jdk1.8.0_301/g' /etc/bash.bashrc
  sed -i 's/jdk1.8.0_271/jdk1.8.0_301/g' /etc/environment
  sed -i 's/jdk1.8.0_271/jdk1.8.0_301/g' /etc/bash.bashrc
  sed -i 's/jdk1.8.0_281/jdk1.8.0_301/g' /etc/environment
  sed -i 's/jdk1.8.0_281/jdk1.8.0_301/g' /etc/bash.bashrc

  IPTABLES=`grep -c "alias iptables" /etc/bash.bashrc`
  if [ $IPTABLES -eq 0 ];then
    echo "alias iptables='echo -e "use nsu_iptables instead of iptables\n"'" >>/etc/bash.bashrc
  fi
  
  IPTABLES=`grep -c "sudo()" /etc/bash.bashrc`
  if [ $IPTABLES -eq 0 ];then
   echo "sudo() { if [[ \$1 == "iptables" ]]; then echo "Use nsu_iptables instead of \$@" ; else command sudo \$@; fi; }" >>/etc/bash.bashrc
  fi

  install_tomcat
  
  cd $IDIR
  
  service tomcat restart
  
  rm -f $HOME_DIR/thirdparty/bin/lz4
  rm /home/cavisson/work/webapps/DashboardServer/WEB-INF/lib/jersey-bundle-1.19.jar >>/dev/null 2>&1

  cp $IDIR/version /etc/.version

  if [ $DISTRO == "Ubuntu" ] ;then
    gitlab_start_for_cavisson
  fi
  cd $IDIR
  mv Cav* $LOG_DIR/
  #echo 'rm -rf !("logs")' | bash -O extglob
  mv $LOG_DIR/Cav* .

  echo "CavBin Upgraded Successfully"
  exit 0

}

install_full_cavbin()
{
  msgout "Install Cavbin"
}

cavbin_installation_mode()
{

if [ ! -z $INSTALLATION ]; then
    if [ "X$INSTALLATION" == "X1" ];then
	install_full_cavbin
    elif [ "X$INSTALLATION" == "X2" ];then
	upgrade_packages
    fi
else
  echo -n "1 for Installation , 2 for Upgrade Packages "
  ANS=`get_input`
  if [ "X$ANS" == "X1" ]; then
    install_full_cavbin;
  elif [ "X$ANS" == "X2" ];then
    upgrade_packages;
  else
    echo "Wrong Input"
    exit 1;
  fi
fi

}

add_iptables_rules()
{
  iptables -t filter -A INPUT -p tcp -m tcp --dport 20:21 -j ACCEPT #ftp
  iptables -t filter -A INPUT -p tcp -m tcp --dport 22 -j ACCEPT #ssh
  iptables -t filter -A INPUT -p tcp -m tcp --dport 80 -j ACCEPT #HTTP
  iptables -t filter -A INPUT -p tcp -m tcp --dport 123 -j ACCEPT #NTP
  iptables -t filter -A INPUT -p tcp -m tcp --dport 443 -j ACCEPT #HTTPS
  iptables -t filter -A INPUT -p tcp -m tcp --dport 587 -j ACCEPT #SMTP secure
  iptables -t filter -A INPUT -p tcp -m tcp --dport 623 -j ACCEPT #tcp warnings and comm port
  iptables -t filter -A INPUT -p udp -m udp --dport 623 -j ACCEPT #udp warnings and comm port
  iptables -t filter -A INPUT -p tcp -m tcp --dport 1024:65535 -j ACCEPT
  #Additional ports are also opened . These are default port for various protocols
  iptables -t filter -A INPUT -p tcp -m tcp --dport 25 -j ACCEPT #SMTP
  iptables -t filter -A INPUT -p tcp -m tcp --dport 53 -j ACCEPT #DNS
  iptables -t filter -A INPUT -p tcp -m tcp --dport 81 -j ACCEPT #HTTP (HPD)
  iptables -t filter -A INPUT -p tcp -m tcp --dport 88 -j ACCEPT #KERBOROS
  iptables -t filter -A INPUT -p tcp -m tcp --dport 110 -j ACCEPT #POP3
  iptables -t filter -A INPUT -p tcp -m tcp --dport 161 -j ACCEPT #SNMP
  iptables -t filter -A INPUT -p tcp -m tcp --dport 873 -j ACCEPT #RSYNC
  iptables -t filter -A INPUT -p tcp -m tcp --dport 990 -j ACCEPT #FTPS
  iptables -t filter -A INPUT -p tcp -m tcp --dport 989 -j ACCEPT #FTPS-DATA
  iptables -t filter -A INPUT -p tcp -m tcp --dport 995 -j ACCEPT #POP3
  iptables -t filter -A INPUT -p tcp -m tcp --dport 68 -j ACCEPT #DHCP
  iptables -t filter -A INPUT -p tcp -m tcp --dport 69 -j ACCEPT #TFTP
  iptables -t filter -A INPUT -p tcp -m tcp --dport 23 -j ACCEPT #TELNET
  iptables -t filter -A INPUT -p tcp -m tcp --dport 993 -j ACCEPT #IMAPS
  iptables -t filter -A INPUT -p tcp -m tcp --dport 514 -j ACCEPT #Syslogd
  iptables -t filter -A INPUT -p tcp -m tcp --dport 67 -j ACCEPT #BOOTP
  iptables -t filter -A INPUT -p tcp -m tcp --dport 137 -j ACCEPT #Net-Bios
  iptables -t filter -A INPUT -p tcp -m tcp --dport 139 -j ACCEPT #SMB(SAMBHA)
  iptables -t filter -A INPUT -p tcp -m tcp --dport 143 -j ACCEPT #IMAP
  iptables -t filter -A INPUT -p tcp -m tcp --dport 389 -j ACCEPT #LDAP
  iptables -t filter -A INPUT -p tcp -m tcp --dport 636 -j ACCEPT #LDAP(s)
  iptables -t filter -A INPUT -p tcp -j REJECT --reject-with icmp-port-unreachable

}

vim_ssh_settings()
{
  #setting highlighting option for searched string
  echo "set hls" >> /etc/vim/vimrc
  #for history to the last position when reopening a file
  echo -e "if has(\"autocmd\")\n\tau BufReadPost * if line(\"'\\\"\") > 1 && line(\"'\\\"\") <= line(\"\$\") | exe \"normal! g'\\\"\" | endif\nendif" >> /etc/vim/vimrc
  #for keeping connections alive
  echo  "ServerAliveInterval 60">>/etc/ssh/ssh_config
}

gitlab-runner_configuration()
{
    #adding gitlab runner configuration for cavisson user
    chmod +x /usr/bin/gitlab-runner >>$CAPTION 2>&1
    mkdir -p /home/gitlab-runner/.gitlab-runner >>$CAPTION 2>&1
    cp /etc/gitlab-runner/config.toml  /home/gitlab-runner/.gitlab-runner/config.toml >>$CAPTION 2>&1
    chown -R cavisson:cavisson /home/gitlab-runner/.gitlab-runner >>$CAPTION 2>&1
    mv /etc/systemd/system/gitlab-runner.service  /etc/systemd/system/gitlab-runner.service.bak >>$CAPTION 2>&1
    gitlab-runner install --user=cavisson --working-directory=/home/gitlab-runner >>$CAPTION 2>&1
    gitlab-runner start >/dev/null 2>&1
    gitlab_adding_cavisson
    if [ $? == 0 ] ;then
        echo "Gitlab runner configuration for cavisson user is done"
    else
        echo "Unable to configure gitlab runner for cavisson user"
    fi
}

gitlab_adding_cavisson()
{
    sed -ir "s/User=/#User=/g" /etc/systemd/system/gitlab-runner.service >>$CAPTION 2>&1
    sed -ir "s/Group=/#Group=/g" /etc/systemd/system/gitlab-runner.service >>$CAPTION 2>&1
    sed -i '14 i User=cavisson' /etc/systemd/system/gitlab-runner.service >>$CAPTION 2>&1
    sed -i '14 a Group=cavisson' /etc/systemd/system/gitlab-runner.service >>$CAPTION 2>&1
    sed -i "s+/etc/gitlab-runner+/home/gitlab-runner+g" /etc/systemd/system/gitlab-runner.service >>$CAPTION 2>&1
    sed -i "s/\"--user\" \"cavisson\"//g" /etc/systemd/system/gitlab-runner.service >>$CAPTION 2>&1
    #gitlab-runner stop >>$CAPTION 2>&1
    #systemctl daemon-reload >>$CAPTION 2>&1
    #gitlab-runner start >>$CAPTION 2>&1
}

gitlab_start_for_cavisson()
{
    systemctl stop gitlab-runner.service >>$CAPTION 2>&1
    systemctl daemon-reload >>$CAPTION 2>&1
    systemctl start gitlab-runner.service >>$CAPTION 2>&1
    if [ $? == 0 ] ;then
        echo "Gitlab runner is running successfully for cavisson user"
    else
        echo "Unable to run gitlab runner for cavisson user"
    fi
}
setting_core_file_path()
{
  echo -e "Updating core file path to /home/cavisson/core_files... "
  # Check cavisson user exist or not
  user_count=`getent passwd cavisson |wc -l` >/dev/null 2>&1
  if [ $user_count ];then
    mkdir -p /home/cavisson/core_files
    chmod 777 /home/cavisson/core_files

    sed -i "/kernel.core_pattern*/d" /etc/sysctl.conf >/dev/null 2>&1
    echo "kernel.core_pattern=|/home/cavisson/bin/nsi_core_pattern %p %s %c %P" >>/etc/sysctl.conf
    # run sysctl --system to reflect these changes 
    sysctl --system
  fi 
  echo -e " Done"
}

get_default_value()
{
 VAR_NAME=$1
 case $VAR_NAME in
 CAV_CTRL_PATH)
     VAR_VALUE=$HOME_DIR/work;;
 esac
}

get_var_value()
{
VAR_NAME=$1
 VAR_VALUE=${!VAR_NAME}
 if [ -z $VAR_VALUE ];then
   #check cav.profile exists or not
   if [ -f $IDIR/cav.profile ];then
     VAR_VALUE=`grep -w "^$VAR_NAME" $IDIR/cav.profile | awk -F "=" '{print $2}'`
     if [ "$VAR_VALUE/work" != "$NS_WDIR" ];then
       mkdir -p $VAR_VALUE/work
       chown cavisson:cavisson $VAR_VALUE/work
       su cavisson -c "ln -s $VAR_VALUE/work $NS_WDIR"
     fi
   fi
 fi
 if [ -z $VAR_VALUE ];then
   get_default_value $VAR_NAME
   echo "WARNING : Controller path is empty so setting by default  path as /home/cavisson."
 elif [ "XX$VAR_VALUE" == "XX/tmp" ];then
   get_default_value $VAR_NAME
   echo "WARNING :Controller Path can not be set as /tmp"
 #checking controller path is not empty neither set to /home/cavisson/work 
 elif [ "XX$VAR_VALUE" != "XX" ];then
   if [ "$VAR_VALUE/work" != "$NS_WDIR" ];then
     mkdir -p $VAR_VALUE/work
     chown cavisson:cavisson $VAR_VALUE/work
     su cavisson -c "ln -s $VAR_VALUE/work $NS_WDIR"
   fi
 fi
 export $VAR_NAME=$VAR_VALUE
}

#####################################################################################################################

export TOMCAT_VERSION=apache-tomcat-9.0.50
CAVCONF=$HOME_DIR/etc/cav.conf
export IDIR=`pwd`
LOG_DIR=$IDIR/logs
mkdir -p $LOG_DIR
CAPTION="$LOG_DIR/cav_install`date +"%m_%d_%Y_%H_%M"`.log"
ADDITIONAL_RPMS_LOGS="$LOG_DIR/cav_additional_rpms.log"
THIRDPARTY_LOGS="$LOG_DIR/cav_thirdparty.log"
HELP_LOGS="$LOG_DIR/cav_help.log"
NS_LOGS="$LOG_DIR/cav_ns.log"
FILE_FOR_SU_PATH="/etc/environment"
BASHRC="/etc/bashrc"

AVAHI_FILE="/etc/default/avahi-daemon"
ZEROCONF_FILE="/etc/sysconfig/network"
AVAHI_AUTOPID_FILE="/etc/network/if-up.d/avahi-autoipd"

#set distro and range of releases for each that are supported
declare -a SUPPORTED_DISTROS=("Redhat" "Ubuntu")
if [[ $DISTRO == "Ubuntu" &&  $RELEASE -eq 1604 ]];then
    declare -a Ubuntu_MIN_REL=("1204" "1604")
    declare -a Ubuntu_MAX_REL=("1204" "1604")
elif [[ $DISTRO == "Ubuntu" &&  $RELEASE -eq 2004 ]];then
    declare -a Ubuntu_MIN_REL=("1204" "2004")
    declare -a Ubuntu_MAX_REL=("1204" "2004")
fi

chmod 777 $IDIR
chmod 755 /root
chmod 777 $LOG_DIR
touch $CAPTION
chmod 777 $CAPTION
touch $NS_LOGS
chmod 777 $NS_LOGS

#this file cavisson installation config is used to completely install NV, NF and & ND
CAV_INSTALL="$IDIR/cavissonInstallation.config"

if [ -f $CAV_INSTALL ];then
    source $CAV_INSTALL
else
    echo "WARNING : $CAV_INSTALL is not present"
fi

touch /home/cavisson/.bashrc >>$CAPTION 2>&1
touch /etc/bash.bashrc >>$CAPTION 2>&1
touch /etc/vim/vimrc >>$CAPTION 2>&1

INSTALL_SPEC_FILE=$IDIR/install.spec

SYS_FILES="ip_entries ip_bonding ip_properties site.env"

ARCH_BITS=$(./nsi_get_linux_release_ex -b)
STATUS=$?

if [ $DISTRO == "Ubuntu" ] ;then
  NS_RELEASE=${DISTRO}${RELEASE}_${ARCH_BITS}
elif [ $DISTRO == "Redhat" ]; then
  NS_RELEASE=${DISTRO}_${ARCH_BITS}
fi

if [[ $DISTRO == "Ubuntu" &&  $RELEASE -eq 1604 ]];then
  POSTGRESQL_VERSION=9.5
elif [ "X$DISTRO" == "XRedhat" ];then
  POSTGRESQL_VERSION=12
elif [[ $DISTRO == "Ubuntu" &&  $RELEASE -eq 2004 ]];then
  POSTGRESQL_VERSION=12
fi

export NS_WDIR=$HOME_DIR/work
export HPD_ROOT=$NS_WDIR/hpd
export DISTRO RELEASE REDHAT_FC_RELEASE ARCH_BITS 
export POSTGRESQL_VERSION
export CAPTION ADDITIONAL_RPMS_LOGS

JAVA_TAR_NAME="jdk-8u301-linux-x64.tar.gz"
export JAVA_DIR="$HOME_DIR/apps/jdk1.8.0_301"
export JAVA_HOME=$JAVA_DIR
export NODEJS_DIR="$HOME_DIR/apps/nodejs"
if [[ $DISTRO == "Ubuntu" &&  $RELEASE -ge 1204 ]];then
  #needed by chkconfig - this is a bug on Ubuntu
  ln -sf /usr/lib/insserv/insserv /sbin/insserv
  #default shell is sh on Ubuntu and this is linked to dash. link to bash instead
  ln -sf /bin/bash /bin/sh
fi

#create ctrl directory at given path and create link if it is other then /home/cavisson/work
get_var_value CAV_CTRL_PATH
echo "CAV_CTRL_PATH"

#Get Postgres conf location based on distro
if [ $DISTRO == "Ubuntu" ] ;then
  POSTGRESQLCONF=/etc/postgresql/$POSTGRESQL_VERSION/main/postgresql.conf
  POSTGRESQL_HBA_CONF=/etc/postgresql/$POSTGRESQL_VERSION/main/pg_hba.conf
elif [ $DISTRO == "Redhat" ];then
  POSTGRESQLCONF=/var/lib/pgsql/12/data/postgresql.conf
  POSTGRESQL_HBA_CONF=/var/lib/pgsql/12/data/pg_hba.conf
fi

#Bug fix as cavisson upgrade was failing due to tomcat dir not set. So exported it
export TOMCAT_DIR="$NS_WDIR/apps/apache-tomcat-9.0.50"
TOMCAT_TAR_NAME="$TOMCAT_VERSION.tar.gz"

check_userid
if [ $DISTRO == "Ubuntu" ] ;then
  init_kernel_version $STATUS
fi
validate_Cav

msgout "----------------------------------------------------------------------"
msgout "Starting NetStorm Installation. Start Date: `date +%m/%d/%y`, Time: `date +%H:%M:%S`" 

cavbin_installation_mode

if [[ $DISTRO == "Ubuntu" ]] ;then
  disable_firewall_ubuntu

  msgout "Adding iptables rules"
  add_iptables_rules

  update-rc.d -f avahi-daemon remove >> /dev/null 2>&1 
  disable_auto_ip_assign "AVAHI_DAEMON_DETECT_LOCAL" "0" "$AVAHI_FILE" >> /dev/null 2>&1
  sed -i "3i exit 0" $AVAHI_AUTOPID_FILE >> /dev/null 2>&1

#making soft link of gmake to make
  ln -s /usr/bin/make /usr/bin/gmake >> /dev/null 2>&1
fi

configure_dns

disable_apparmor_ubuntu

set_default_timezone

if [[ $DISTRO == "Ubuntu" ]] ;then
  #modify grub option for boot in text mode.
  modify_grub_for_text_mode

  echo "installing additional pkgs for Ubuntu"
  if [ -f /etc/mongod.conf ];then
      rm /etc/mongod.conf >>$CAPTION 2>&1
  fi
  install_additional_pkgs_ubuntu

  disable_network_manager_ubuntu
fi

create_cavmodem_device
create_cavisson_user
create_cav_conf
create_domain_admin_file
create_test_run_and_ts_run_id

#change the directory  to "current directory"
msgout "Changing the directory to $IDIR"
if [ ! -d $IDIR ];then
  msgout "$IDIR does not exist"
  exit 1
fi
cd $IDIR

if [ $DISTRO == "Ubuntu" ] ;then 
  install_ns_database
elif [ $DISTRO == "Redhat" ] ;then
  install_ns_database_for_redhat
fi

increase_fdsetsize
configure_snmp_agent

if [ $DISTRO == "Ubuntu" ] ;then
    gitlab-runner_configuration
fi

add_delete_services
install_ssl_file

install_java_and_tomcat
install_node_js

if [ $DISTRO == "Ubuntu" ] ;then 
  modify_path_for_su
fi
install_thirdparty_components

install_ns_components

copy_scripts_scenario

add_entries_to_etc_host

msgout "Configuring auto-start of services"
$NS_WDIR/bin/nsu_configure -n >> $CAPTION 2>&1

if [ $DISTRO == "Ubuntu" ] ;then
  msgout "Configuring language locally"
 
  if [[ $RELEASE -eq 1604 || $RELEASE -eq 2004 ]] ;then
    option=--frontend=noninteractive
  fi
 
  dpkg-reconfigure ${option} locales >>$CAPTION 2>&1
  update-locale LANGUAGE=en_US.UTF-8:en LANG=en_US.UTF-8 LC_ALL=en_US.UTF-8  >>$CAPTION 2>&1
  locale-gen --purge --no-archive >>$CAPTION 2>&1
fi

copy_at_file_SQ

start_postgres_clusters

vim_ssh_settings

setting_core_file_path >>$CAPTION 2>&1

create_ns_services >>$CAPTION 2>&1

rm /home/cavisson/work/webapps/DashboardServer/WEB-INF/lib/jersey-bundle-1.19.jar >>/dev/null 2>&1

RESERVED_SHELL_PATH=$NS_WDIR/bin/nsu_reserve_port
$RESERVED_SHELL_PATH -p "7890-7899,8000-8020"

IPTABLES=`grep -c "alias iptables" /etc/bash.bashrc`
if [ $IPTABLES -eq 0 ];then
  echo "alias iptables='echo -e "use nsu_iptables instead of iptables"'" >>/etc/bash.bashrc
  echo "sudo() { if [[ \$1 == "iptables" ]]; then echo "Use nsu_iptables instead of \$@" ; else command sudo \$@; fi; }" >>/etc/bash.bashrc
fi

$NS_WDIR/tools/nsu_iptables -L >> $CAPTION 2>&1
chown cavisson:cavisson $NS_WDIR/etc/iptables/rules.v4
bash $NS_WDIR/tools/rm_wrapper.sh -u >> $CAPTION 2>&1
bash $NS_WDIR/tools/rm_wrapper.sh -i >> $CAPTION 2>&1
cp $IDIR/version /etc/.version
if [ $DISTRO == "Ubuntu" ] ;then
    gitlab_start_for_cavisson
fi
cd $IDIR
mv Cav* $LOG_DIR/
#echo 'rm -rf !("logs")' | bash -O extglob
mv $LOG_DIR/Cav* .
if [[ $DISTRO == "Ubuntu" &&  $RELEASE == "2004" ]];then
echo -e "AllowSuspend=no \nAllowHibernation=no \nAllowSuspendThenHibernate=no \nAllowHybridSleep=no">> /etc/systemd/sleep.conf
fi
if [ ! -z $CONTROLLER ];then
    if [ -f $NS_WDIR/tools/nsu_manage_controller ];then
	echo "going to create controller as per $CAV_INSTALL file"
	touch $NS_WDIR/nsu_manage_controller.log
	chown cavisson:cavisson $NS_WDIR/nsu_manage_controller.log
	chmod 777 $NS_WDIR/nsu_manage_controller.log
    	su cavisson -c "source /home/cavisson/work/etc/netstorm.env && source $CAV_INSTALL && bash $NS_WDIR/tools/nsu_manage_controller -o add -n Controller_$CONTROLLER -c $CONTROLLER -P \"TOMCAT_IP=$system_ip;TOMCAT_HTTP_PORT=8048;TOMCAT_HTTPS_PORT=4431;TOMCAT_SHUTDOWN_PORT=8250;LPS_LISTEN_PORT=8249;RECORDER_PORT=12001-12011;HPD_SERVER_ADDRESS=$system_ip;HPD_PORT=9002;HPD_SPORT=9003;HPD_FTP_PORT=9004;HPD_SMTP_PORT=9005;HPD_POP3_PORT=9006;HPD_DNS_PORT=9007;JAVA_SERVER_TCP_PORT=7050;JAVA_SERVER_UDP_PORT=7249\"" >>$CAPTION 2>&1
	if [ $? -eq 0 ];then
	    echo "$CONTROLLER created successfully as per $CAV_INSTALL file"
	else
	    echo "unable to create controller via $CAV_INSTALL file"
	fi
    else
	echo "$NS_WDIR/tools/nsu_manage_controller file does not exist"
    fi
else
    echo "no controller will be created, $CAV_INSTALL file not present"
fi

msgout "Cavbin installation is Complete. End Date: `date +%m/%d/%y`, Time: `date +%H:%M:%S`"
msgout "Run post_install.sh. If you are  using netstorm and netocean pair, then both machines must be up before running post_install.sh"
msgout "----------------------------------------------------------------------"

exit 0
