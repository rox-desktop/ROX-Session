"""This file is not processed by ROX-Session unless it is copied to either
$HOME/.config/rox.sourceforge.net/ROX-Session/Environment.py or
/etc/xdg/rox.sourceforge.net/ROX-Session/Environment.py
If Environment.py is found in either of those locations it will be loaded
by ROX-Session when initializing, with the $HOME/.config file taking
precedence over the /etc/xdg file if both exist.  If either file is
world-writable it is ignored.

The function of this file is to set up environment variables for the user's
session.

A # character in this file starts a comment which lasts until the end of
the line.  Some example commands are below, most commented out.  Remove the
# character to use the command."""

# Get some useful functions.  This defines the get(), set(), set_path(),
# append_path() and other functions.
from env_helper import *

# Look up the home directory.
home=get('HOME')

# Example: set up LD_LIBRARY_PATH to have the ~/lib directory as the first
# entry.
#prepend_path('LD_LIBRARY_PATH', '~/lib')
#
# Example set up $PATH to have the user's private bin directory as the last
# place to look.
#append_path('PATH', '~/bin')

# Uncomment this line to have ROX-Session start a ssh-agent daemon for
# your session.  ssh-agent makes using ssh easier.
#start_ssh_agent()
