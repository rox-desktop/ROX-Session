import sys, os
from logging import info

import dbus_compat
sys.modules['dbus.services'] = dbus_compat

dbus_version = 0
try:
	from dbus import *
	if hasattr(dbus, 'version') and dbus.version >= (0, 40, 0):
		dbus_version = 3
		dbus_daemon = 'dbus-daemon'
		info("D-BUS 0.3x detected")
	else:
		dbus_version = 2
		dbus_daemon = 'dbus-daemon-1'
		info("D-BUS 0.2x detected")
except ImportError:
	# Debian/unstable doesn't have a version of D-BUS for their
	# default version of Python. Try the 2.4 bindings..
	if sys.version_info[0] == 2 and sys.version_info[1] < 4:
		print "Restarting with Python 2.4..."
		try:
			os.execlp('python2.4', 'python2.4', *sys.argv[:])
		except:
			print >>sys.stderr, "Failed to import D-BUS bindings, " \
				"and retrying with Python 2.4 didn't work."

	rox.alert("Failed to import dbus module. You probably need "
		"to install a package with a name like 'python%s.%s-dbus'.\n\n"
		"D-BUS can also be downloaded from http://freedesktop.org\n"
		"(be sure to compile the python and glib bindings)\n\n"
		"If the bindings are installed in /usr/local/lib/python...,"
		"try moving them to /usr/lib/python... (without the 'local')" %
		(sys.version_info[0], sys.version_info[1]))
	raise
