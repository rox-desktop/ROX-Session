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

# Example: set up LD_LIBRARY_PATH
#os.putenv('LD_LIBRARY_PATH',
#          os.path.join(os.getenv('HOME'), 'lib')+
#          os.getenv('LD_LIBRARY_PATH', '/usr/local/lib:/usr/lib')

# Ensure ssh-agent is running
def start_ssh_agent():
    ssh_dir=os.path.join(os.getenv('HOME'), '.ssh')
    if os.isdir(ssh_dir):
        if not os.getenv('SSH_AGENT_PID'):
            ssh_cache=os.path.join(os.getenv('HOME'), '.ssh-agent')
            os.system('ssh-agent -s > '+ssh_cache)

            vars={}
            for line in file(ssh_cache, 'r'):
                for cmd in line.strip().split(';'):
                    if '=' in cmd:
                        var, val=cmd.split('=', 1)
                        vars[var]=val

                    else:
                        exp, var=cmd.strip().split()
                        if exp=='export':
                            try:
                                os.putenv(var, vars[var])
                            except:
                                pass

#start_ssh_agent()
