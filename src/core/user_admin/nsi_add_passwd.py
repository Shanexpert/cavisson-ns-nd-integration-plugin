#!/usr/bin/env python
import pexpect
import getpass
import time
import os
import sys

def cli_login(user, password):
    child = pexpect.spawn('passwd %s'%(user))
    i = child.expect([pexpect.TIMEOUT, 'password:'])
    if i == 0: # Timeout
        print 'SSH could not login:'
        print child.before, child.after
        return None
    child.sendline(password)
    i = child.expect([pexpect.TIMEOUT, 'password:','#'])    # Expect CLI prompt
    if i == 0: # Timeout
        return None
    if i == 1: #retype
	child.sendline(password)
    print child.before, child.after
    return child

def main ():
    if len(sys.argv) < 3:
        print ('USAGE: The value is a mandatory argument')
        sys.exit(1)
    username = sys.argv[1]
    password = sys.argv[2] 

    child = cli_login(username, password)
    print 'Password changed'
    return 0
    
if __name__ == '__main__':
    main()
