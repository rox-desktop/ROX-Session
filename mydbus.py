import sys, os
from logging import info

import dbus_compat
sys.modules['dbus.services'] = dbus_compat

dbus_version = 0
broken_dbus3x = False
version = (0, 0, 0)
try:
	from dbus import *
	if version >= (0, 40, 0):
		dbus_version = 3
		dbus_daemon = 'dbus-daemon'
		if version <= (0, 42, 0):
			broken_dbus3x = True
		info("D-BUS 0.3x detected")
	else:
		dbus_version = 2
		dbus_daemon = 'dbus-daemon-1'
		info("D-BUS 0.2x detected")
except ImportError:
	# Don't really care, now we have XML-RPC support.
	dbus_version = None
	print ("Failed to import dbus module. You probably need "
		"to install a package with a name like 'python%s.%s-dbus'.\n\n"
		"D-BUS can also be downloaded from http://freedesktop.org\n"
		"(be sure to compile the python and glib bindings)\n\n"
		"If the bindings are installed in /usr/local/lib/python...,"
		"try moving them to /usr/lib/python... (without the 'local')" %
		(sys.version_info[0], sys.version_info[1]))
