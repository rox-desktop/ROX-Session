import sys, os
from logging import info

import dbus_compat
sys.modules['dbus.services'] = dbus_compat
import dbus
import constants

session_bus = None

try:
	import dbus
	if hasattr(dbus, 'version') and dbus.version >= (0, 40, 0):
		dbus_version = 3
		daemon = 'dbus-daemon'
		info("D-BUS 0.3x detected")
	else:
		dbus_version = 2
		daemon = 'dbus-daemon-1'
		info("D-BUS 0.2x detected")
except ImportError:
	rox.alert("Failed to import dbus module. You probably need "
		"to install a package with a name like 'python2.3-dbus'.\n\n"
		"D-BUS can also be downloaded from http://freedesktop.org\n"
		"(be sure to compile the python and glib bindings)\n\n"
		"If the bindings are installed in /usr/local/lib/python...,"
		"try moving them to /usr/lib/python... (without the 'local')")
	raise

def init():
	global session_bus
	if 'DBUS_SESSION_BUS_ADDRESS' not in os.environ:
		info('No session bus running... attempting to start one')
		r, w = os.pipe()
		child = os.fork()
		if child == 0:
			# We are the child
			try:
				os.close(r)
				os.execlp(daemon, daemon, '--session', '--print-address=%d' % w)
			finally:
				print >>sys.stderr, "Failed to exec", daemon
				sys.stderr.flush()
				#os._exit(1)
		os.close(w)
		addr = ''
		while '\n' not in addr:
			extra = os.read(r, 100)
			if not extra: raise Exception('Failed to read D-BUS address!')
			addr += extra
		addr = addr[:addr.index('\n')]
		os.close(r)
		info('Started bus with address: "%s"', addr)
		os.environ['DBUS_SESSION_BUS_ADDRESS'] = addr
	else:
		info("A D-BUS session bus is already running. Using that.")
	session_bus = dbus.SessionBus()

	# XXX: kill dbus on exit
