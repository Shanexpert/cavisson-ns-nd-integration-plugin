#!/bin/sh

file_flag=0

show_error_and_exit()
{
  echo -e "Error: $*"
  exit 1
}

while [ "$1" != "" ]
do
case $1 in
-f | --jmx_file)
       shift
       FILE=$1
       file_flag=$(expr "$file_flag" + 1);;

* | ?)
      show_error_and_exit "Invalid Argument '$1'"
       exit 1;;

esac
shift
done

java -Djava.awt.headless=true -cp .:$JMETER_HOME/lib/*:$JMETER_HOME/lib/ext/*:$NS_WDIR/webapps/netstorm/lib/jmeter-plugins-manager-0.19.jar:$NS_WDIR/webapps/netstorm/lib/scriptconverter.jar -DjmeterFilePath=$FILE -DjmeterListenerClass=com.cavisson.backend.CavJMBackendListener -DjmeterHomeDir=$JMETER_HOME -Dlog4j.configurationFile=$NS_WDIR/webapps/sys/log4j.properties com.cavisson.JmxExecutor.AttachListenerToJmx
