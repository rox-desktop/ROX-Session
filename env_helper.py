# Useful functions for setting the users environment.  The example
# Environment.py file uses these functions.

import os, sys

home=os.getenv('HOME')

def get(var):
    try:
        return os.environ[var]
    except:
        pass

def set(var, val):
    val=os.path.expanduser(val)
    os.environ[var] = val

def set_path(var, *vals):
    vals=map(os.path.expanduser, vals)
    os.environ[var] = ':'.join(vals)

def append_path(var, *vals):
    vals=map(os.path.expanduser, vals)
    try:
        old=os.environ[var]
    except:
        old=None
    if old:
        os.environ[var] = old+':'+(':'.join(vals))
    else:
        os.environ[var] = ':'.join(vals)

def prepend_path(var, *vals):
    vals=map(os.path.expanduser, vals)
    try:
        old=os.environ[var]
    except:
        old=None
    if old:
        os.environ[var] = (':'.join(vals))+':'+old
    else:
        os.environ[var] = ':'.join(vals)

# Ensure ssh-agent is running
def parse_ssh_agent_cache():
    """If ~/.ssh-agent exits, read and parse it to get the ssh-agent
    settings.  Used by start_ssh_agent()."""
    ssh_cache=os.path.join(home, '.ssh-agent')
    if not os.access(ssh_cache, os.R_OK):
        return

    vars={}
    for line in file(ssh_cache, 'r'):
        for cmd in line.strip().split(';'):
            if '=' in cmd:
                var, val=cmd.split('=', 1)
                vars[var]=val

            elif cmd.strip():
                exp, var=cmd.strip().split(None, 1)
                if exp=='export':
                    try:
                        os.environ[var]=vars[var]
                    except:
                        pass
    
def start_ssh_agent():
    """Start the ssh-agent daemon if it is not running."""
    ssh_dir=os.path.join(home, '.ssh')
    if os.path.isdir(ssh_dir):
        if not os.getenv('SSH_AGENT_PID'):
            try:
                parse_ssh_agent_cache()
            except:
                pass

        try:
            sockname=os.environ['SSH_AUTH_SOCK']
        except:
            sockname=None

        if not sockname or not os.access(sockname, os.R_OK):
            ssh_cache=os.path.join(home, '.ssh-agent')
            os.system('ssh-agent -s > '+ssh_cache)
            os.chmod(ssh_cache, 0600)

            parse_ssh_agent_cache()

if __name__=='__main__':
    print home, get('HOME')
    print get('PATH')
    prepend_path('PATH', '~/bin')
    print get('PATH')
    
