"""This file is not processed by ROX-Session unless it is copied to either
$HOME/.config/rox.sourceforge.net/ROX-Session/Environment.py or
/etc/xdg/rox.sourceforge.net/ROX-Session/Environment.py
If Environment.py is found in either of those locations it will be loaded
by ROX-Session when initializing, with the $HOME/.config file taking
precedence over the /etc/xdg file if both exist.  If either file is
world-writable it is ignored.

The function of this file is to set up environment variables for the user's
session."""

import os

home=os.getenv('HOME')

# Example: set up LD_LIBRARY_PATH
#os.putenv('LD_LIBRARY_PATH',
#          os.path.join(home, 'lib')+
#          os.getenv('LD_LIBRARY_PATH', '/usr/local/lib:/usr/lib')

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

# Uncomment this line to have ROX-Session start a ssh-agent daemon for
# your session.  ssh-agent makes using ssh easier.
#start_ssh_agent()
