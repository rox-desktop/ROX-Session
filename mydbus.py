import sys, os

import dbus_compat
sys.modules['dbus.services'] = dbus_compat

try:
	from dbus import *
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
			raise
