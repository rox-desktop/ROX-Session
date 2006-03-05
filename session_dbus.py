import sys, os
from logging import info
import mydbus as dbus

import constants

_session_bus = None

def init():
	global _session_bus
	if 'DBUS_SESSION_BUS_ADDRESS' not in os.environ:
		info('No session bus running... attempting to start one')
		r, w = os.pipe()
		child = os.fork()
		if child == 0:
			# We are the child
			try:
				os.close(r)
				os.execlp(dbus.dbus_daemon, dbus.dbus_daemon,
					'--session', '--print-address=%d' % w)
			finally:
				print >>sys.stderr, "Failed to exec", dbus.dbus_daemon
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
	get_session_bus()

	# XXX: kill dbus on exit

def get_session_bus():
	global _session_bus
	if not dbus.dbus_version_ok: return None
	if not _session_bus:
		_session_bus = dbus.SessionBus()
	return _session_bus
