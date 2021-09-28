#!/usr/bin/python3
import psycopg2
from psycopg2.extras import RealDictCursor
import json
import datetime

class DateTimeEncoder(json.JSONEncoder):
    def default(self, o):
        if isinstance(o, datetime):
            return o.ctime()
        return json.JSONEncoder.default(self, o)

def create_db_connection(db_name, db_user, db_host, db_port):
    conn = psycopg2.connect(database = db_name, user = db_user, host = db_host, port = db_port)
    cur = conn.cursor(cursor_factory=RealDictCursor)
    return cur, conn;

