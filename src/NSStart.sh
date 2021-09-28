#
# Name: NSStart.sh
# Author: Neeraj Jain
# Purpsose: To start netstorm from GUI
#
# Usage:  NSStart.sh <Scenario Name>
#   e.g.  NSStart.sh TestScenario
#
# Modification History:
#   11/23/04: Neeraj - Initial Version
#

. /home/cavisson/.bashrc

if [ "XX" = "XX$NS_WDIR" ]
then
  NS_WDIR=/home/cavisson/work
fi

if [ $# != 1 ]
then
  echo "Usage:"
  echo "NSStart.sh <Scenario Name>"
  echo "    e.g.  NSStart.sh TestScenario"
  exit -1
fi


ScenPath=$NS_WDIR/scenarios
ScenFileName=scenarios/$1.conf
LogFile=/tmp/NSStart.log

echo "NSStart - `date`" > $LogFile
cd $NS_WDIR

if [ ! -f  $ScenFileName ]
then
  echo "Scenario file not found, $ScenFileName"
  exit -1
fi

nohup bin/netstorm -c $ScenFileName 1>> $LogFile 2>&1 &

exit 0
