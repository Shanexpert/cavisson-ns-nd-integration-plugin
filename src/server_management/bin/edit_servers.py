#!/usr/bin/python3

import os
import sys
import psycopg2
from psycopg2.extras import RealDictCursor
import logging
import time
import json
from datetime import datetime
from configparser import ConfigParser
import commons

db_name = 'demo'
db_user = 'postgres'
db_host = 'localhost'
db_port = '5432'
ns_wdir = os.environ["NS_WDIR"]
tmpdir = ns_wdir + "/server_management/tmp"
confdir = ns_wdir + "/server_management/conf"
logfile = ns_wdir + "/server_management/logs/ncp.log"
editlog = ns_wdir + "/server_management/logs/addDelete.log"
defined_allocation = ['Dedicated','Additional','Free','Reserved']
defined_machinetype = ['Generator','Controller','Netstorm','NO','Netocean']
now = datetime.now().ctime()

try:
    f = open(editlog, "a")
except Exception as e:
    print ("1\nError opening log file " + editlog , sys.stderr)
    exit(1)

if len(sys.argv) != 7:
    f.write("%s|%s|Incorrect number of arguments\n" %(sessuser, now))
    print("1\nERROR! Incorrect number of arguments", file = sys.stderr)
    exit (1)
server = sys.argv[1]
blade = sys.argv[2]
allocation = sys.argv[3]
machinetype = sys.argv[4]
securitygroup = sys.argv[5]
sessuser = sys.argv[6]

try:
    defined_securitygroups = []
    conf = ConfigParser()
    conf.read(confdir + "/NCP.conf")
    for key in conf['securityGroups']:
        defined_securitygroups.append(key)
except Exception as e:
    f.write("%s|%s|Unable to get security groups.\n" %(sessuser, now))
    print("1\nUnable to get security groups.", file = sys.stderr)
    print(e, file = sys.stderr)
    exit (1)

if not securitygroup in defined_securitygroups:
    f.write("%s|%s|Incorrect value of Security Group.\n" %(sessuser, now))
    print("1\nIncorrect value of Security Group.\nPossible values are " + ' '.join(map(str, defined_securitygroups)), file = sys.stderr)
    exit(1)
if not allocation in defined_allocation:
    f.write("%s|%s|Incorrect value of allocation.\n" %(sessuser, now))
    print("1\nIncorrect value of allocation.\nPossible values are " + ' '.join(map(str, defined_allocation)), file = sys.stderr)
    exit(1)
if not machinetype in defined_machinetype:
    f.write("%s|%s|Incorrect value of machine type.\n" %(sessuser, now))
    print("1\nIncorrect value of machine type.\nPossible values are " + ' '.join(map(str, defined_machinetype)), file = sys.stderr)
    exit(1)

class DateTimeEncoder(json.JSONEncoder):
    def default(self, o):
        if isinstance(o, datetime):
            return o.ctime()
        return json.JSONEncoder.default(self, o)

def get_params_from_db(query, cur, conn, param):
    cur.execute(query)
    res = json.loads(json.dumps(cur.fetchall(), cls=DateTimeEncoder, indent=2))
    if bool(res):
        return res[0][param]
    else:
        return None
try:
    cur, conn = commons.create_db_connection(db_user=db_user, db_name=db_name, db_host=db_host, db_port=db_port)
except Exception as e:
    f.write("%s|%s|Unable to connect to database %s at host %s port %s\n" %(sessuser, now, db_name, db_host, db_port))
    print ("1\nUnable to connect to database %s at host %s port %s" %(db_name, db_host, db_port), file = sys.stderr)
    exit(1)

query = "SELECT server_name FROM allocation WHERE LOWER(server_name)=LOWER('%s')" % (server)
val_server = get_params_from_db(query=query, cur=cur, conn=conn, param='server_name')
if val_server != None:
    query = "SELECT blade_name FROM allocation WHERE LOWER(blade_name)=LOWER('%s') AND server_name='%s'" % (blade, server)
    val_blade = get_params_from_db(query=query, cur=cur, conn=conn, param='blade_name')
    if val_blade != None:
        query = "SELECT allocation FROM allocation WHERE server_name='%s' AND blade_name='%s'" % (server, blade)
        val_allocation = get_params_from_db(query=query, cur=cur, conn=conn, param='allocation')
        query = "SELECT machine_type FROM allocation WHERE server_name='%s' AND blade_name='%s'" % (server, blade)
        val_machinetype = get_params_from_db(query=query, cur=cur, conn=conn, param='machine_type')
        query = "SELECT CASE WHEN security_group is null OR security_group='' THEN 'generator' ELSE security_group END FROM servers WHERE server_name='%s'" % (server)
        val_securitygroup = get_params_from_db(query=query, cur=cur, conn=conn, param='security_group')
        if (allocation == val_allocation) and (machinetype == val_machinetype) and (securitygroup == val_securitygroup):
            f.write("%s|%s|%s:%s No state change.\n" %(sessuser, now, server, blade))
            print("0\nNo state change", file = sys.stdout)
            exit(0)
        if allocation != val_allocation:
            query = "UPDATE allocation SET allocation='%s' WHERE server_name='%s' AND blade_name='%s'" % (allocation, server, blade)
            try:
                cur.execute(query)
                conn.commit()
                f.write("%s|%s|%s:%s Allocation changed from %s to %s\n" %(sessuser, now, server, blade, val_allocation, allocation))
                print("0\n%s:%s Allocation changed from %s to %s" %(server, blade, val_allocation, allocation), file = sys.stdout)
            except Exception as e:
                print("1\nError updating allocation status")
                print (e)

        if machinetype != val_machinetype:
            query = "UPDATE allocation SET machine_type='%s' WHERE server_name='%s' AND blade_name='%s'" % (machinetype, server, blade)
            try:
                cur.execute(query)
                conn.commit()
                f.write("%s|%s|%s:%s Machine Type changed from %s to %s\n" %(sessuser, now, server, blade, val_machinetype, machinetype))
                print("0\n%s:%s Machine Type changed from %s to %s" % (server, blade, val_machinetype, machinetype))
            except Exception as e:
                print("1\nError updating machine type")
                print (e)

        if securitygroup != val_securitygroup:
            query = "UPDATE servers SET security_group='%s' where server_name='%s'" % (securitygroup, server)
            try:
                cur.execute(query)
                conn.commit()
                f.write("%s|%s|%s:%s Security Group changed from %s to %s\n" %(sessuser, now, server, blade, val_securitygroup, securitygroup))
                print("0\n%s:%s Security Group changed from %s to %s" % (server, blade, val_securitygroup, securitygroup))
            except Exception as e:
                print("1\nError updating security group")
                print (e)
    else:
        f.write("%s|%s|Blade %s does not exist for %s\n" %(sessuser, now, blade, server))
        print("1\nBlade %s does not exist for server %s" % (blade, server))
        conn.close()
        exit(1)
else:
    f.write("%s|%s|Server %s does not exist\n" %(sessuser, now, server))
    print("1\nServer %s does not exist" % (server))
    conn.close()
    exit (1)
f.close()
conn.close()
exit (0)
