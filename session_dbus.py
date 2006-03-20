import sys, os
import signal
from logging import info
import mydbus as dbus

import constants

_session_bus = None
_dbus_pid = None

def init():
	global _session_bus
	global _dbus_pid
	if 'DBUS_SESSION_BUS_ADDRESS' not in os.environ:
		info('No session bus running... attempting to start one')
		r, w = os.pipe()
		_dbus_pid = os.fork()
		if _dbus_pid == 0:
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
			if not extra:
				if not dbus.dbus_version_ok:
					break
				raise Exception('Failed to read D-BUS address!')
			addr += extra
		addr = addr[:addr.index('\n')]
		os.close(r)
		if addr:
			info('Started bus with address: "%s", PID %d', addr,
			     _dbus_pid)
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

def destroy():
	global _dbus_pid
	if _dbus_pid is not None:
		info("Killing D-BUS daemon process %d", _dbus_pid)
		try:
			os.kill(_dbus_pid, signal.SIGTERM)
		except OSError, exc:
			info("Could not kill daemon: %s", str(exc))
	_dbus_pid = None
