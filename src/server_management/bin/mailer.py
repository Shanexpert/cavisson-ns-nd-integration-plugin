#!/usr/bin/python3
import fcntl
import os
import sys
import psycopg2
from psycopg2.extras import RealDictCursor
import logging
import time
import json
from datetime import datetime
from configparser import ConfigParser
import smtplib, ssl
from email.mime.text import MIMEText
from email.mime.multipart import MIMEMultipart
from jinja2 import Environment, FileSystemLoader

db_name = 'demo'
db_user = 'postgres'
db_host = 'localhost'
db_port = '5432'
ns_wdir = os.environ["NS_WDIR"]
tmpdir = ns_wdir + "/server_management/tmp"
confdir = ns_wdir + "/server_management/conf"
logfile = ns_wdir + "/server_management/logs/mailer.log"
jinja_template_dir = ns_wdir + "/server_management/template"
lockfile = ns_wdir + "/server_management/tmp/.mailer.lock"
pidfile = ns_wdir + "/server_management/tmp/.mailer.pid"
logging.basicConfig(filename = logfile, filemode='a', format='%(levelname)s|%(asctime)s|%(name)s|%(message)s')

lck = open(lockfile, 'w')
p = open(pidfile, 'a')
try:
    fcntl.lockf(lck, fcntl.LOCK_EX | fcntl.LOCK_NB)
    p.write(str(os.getpid()))
    p.close()
except IOError:
    print ("ERROR: An instance of this programme is already running. Exiting")
    sys.exit(0)

# Setting config variables
try:
    conf = ConfigParser()
    conf.read(confdir + "/NCP.conf")
    firstmail = conf.getint('mailer', 'firstmail_status')
    nextmail = conf.getint('mailer', 'nextmail_status')
    To = conf.get('mailer', 'To').strip()
    From = conf.get('mailer', 'From').strip()
    smtphost = conf.get('mailer', 'smtphost').strip()
    smtpport = conf.get('mailer', 'smtpport').strip()
    smtpuser = conf.get('mailer', 'smtpuser').strip()
    smtppass = conf.get('mailer', 'smtppass').strip()
    domainname = conf.get('mailer', 'domainname').strip()
except Exception as e:
    print (e)
    exit(1)

def sendemail(smtp_server, port, user, password, sender_email, receiver_email, subject, html):
    context = ssl.create_default_context()
    message = MIMEMultipart("alternative")
    message["Subject"] = subject
    message["From"] = sender_email
    message["To"] = receiver_email
    reciver_list = receiver_email.split(',')
    message.attach(MIMEText(html, "html"))
    try:
        server = smtplib.SMTP(smtp_server,port)
        server.starttls(context=context)
        server.login(user, password)
        server.sendmail(user, reciver_list, message.as_string())
        return (0)
    except Exception as e:
        print(e)
        return(1)
    finally:
        server.quit()

def duration(time):
    day = time // (24 * 3600)
    time = time % (24 * 3600)
    hour = time // 3600
    time %= 3600
    minutes = time // 60
    time %= 60
    seconds = time
    if day > 0:
        duration = "%d days, %d:%d:%d" % (day, hour, minutes, seconds)
    else:
        if hour > 0:
            duration = "%d:%d:%d" % (hour, minutes, seconds)
        else:
            duration = "%d:%d" % (minutes, seconds)
    return duration

def createhtml(mode, data, status):
    env = Environment(loader=FileSystemLoader(jinja_template_dir))
    if mode == 1:
        template = env.get_template('services_up.html.j2')
    elif mode == 2:
        template = env.get_template('services_down.html.j2')
    elif mode == 3:
        template = env.get_template('security_report.html.j2')
    output = template.render(mode=mode, data=data, status=status)
    return output

### To evaluate datatime object returned by psql
class DateTimeEncoder(json.JSONEncoder):
    def default(self, o):
        if isinstance(o, datetime):
            return o.ctime()
        return json.JSONEncoder.default(self, o)
## End ##

def create_db_connection():
    conn = psycopg2.connect(database = db_name, user = db_user, host = db_host, port = db_port)
    cur = conn.cursor(cursor_factory=RealDictCursor)
    return cur, conn;
################################################ main ##############################

while True:

# Creating database connection for complete loop
    try:
        cur, conn = create_db_connection()
    except Exception as e:
        logging.error("Unable to connect to database %s at host %s port %s", db_name, db_host, db_port)
        print(e)
        continue
###################### end ######################

##### Check Servers Up/Down stats ################
    current_epoch = time.time()
    query = """ SELECT server_ip, downtime_epoch, downtime, CASE WHEN next_mail is null THEN 0 ELSE next_mail END, server_name
    FROM billing_cc
    WHERE (
      (((SELECT extract(epoch from now()) - downtime_epoch) > (SELECT firstmail from mail_params limit 1) AND next_mail is null)
      OR (SELECT extract(epoch from now()) > next_mail) AND next_mail is not null)
      AND status='f'
    )
    """
    try:
        cur.execute("UPDATE mail_params SET firstmail='%d', nextmail='%d'" %(firstmail, nextmail))
        conn.commit()
        cur.execute(query)
        res = json.loads(json.dumps(cur.fetchall(), cls=DateTimeEncoder, indent=2))
    except Exception as e:
        logging.error(e)
        continue

    for i in res:
        i['duration'] = duration(current_epoch - i['downtime_epoch'])
        i['subsequentmail'] = current_epoch + nextmail
    if bool(res):
        message = createhtml(mode=2, data=res, status="Service Interrupted..")
        subject = "Services got Interrupted. Attention required."
        if (sendemail(smtp_server=smtphost, port=smtpport, user=smtpuser, password=smtppass, sender_email=From, receiver_email=To, subject=subject, html=message) == 0):
            for i in res:
                try:
                    cur.execute("UPDATE billing_cc SET next_mail='%d' where server_ip='%s' and status='f'" %(i['subsequentmail'], i['server_ip']))
                    conn.commit()
                except Exception as e:
                    logging.error(e)

    try:
        cur.execute("SELECT server_ip, uptime, TO_CHAR(duration, 'HH24:MI:SS') AS duration, next_mail, server_name FROM billing_cc WHERE status='t' AND next_mail is not null")
        res = json.loads(json.dumps(cur.fetchall(), cls=DateTimeEncoder, indent=2))
    except Exception as e:
        logging.error(e)
        continue

    if bool(res):
        message = createhtml(mode=1, data=res, status="Service Continued..")
        subject = "Services Continued.."
        if (sendemail(smtp_server=smtphost, port=smtpport, user=smtpuser, password=smtppass, sender_email=From, receiver_email=To, subject=subject, html=message) == 0):
            for i in res:
                try:
                    cur.execute("update billing_cc set next_mail = null where server_ip='%s' and next_mail='%d'" %(i['server_ip'], i['next_mail']))
                    conn.commit()
                except Exception as e:
                    logging.error(e)


################### End ##################
################# Nmap Scan alerts #################
    query = """ SELECT server_name, server_ip, start_time, report_file, security_group, unnest(vport) as vport, unnest(vport_service) as vport_service, unnest(vport_service_version) as vport_service_version
    FROM security_scan
    WHERE ismailsent = FALSE
    """
    try:
        cur.execute(query)
        res = json.loads(json.dumps(cur.fetchall(), cls=DateTimeEncoder, indent=2))
    except Exception as e:
        logging.error(e)
        continue

    if bool(res):
        for i in res:
            i['reportlink'] = "https://" + domainname + "/logs/nmap/reports" + i['report_file']
        message = createhtml(mode=3, data=res, status="Undefined Ports Open..")
        subject = "Attention Required!! Netcloud Security Scan Report Card"
        if (sendemail(smtp_server=smtphost, port=smtpport, user=smtpuser, password=smtppass, sender_email=From, receiver_email=To, subject=subject, html=message) == 0):
            for i in res:
                try:
                    cur.execute("UPDATE security_scan SET ismailsent='true' WHERE server_ip='%s' AND ismailsent = FALSE"  %(i['server_ip']))
                    conn.commit()
                except Exception as e:
                    raise
#################### End ###########################
    conn.close()
    time.sleep(60)
