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
				try:
					os.execlp(dbus.dbus_daemon, dbus.dbus_daemon,
						'--session', '--print-address=%d' % w)
				except OSError:
					raise
				except:
					import traceback
					traceback.print_exc()
			finally:
				print >>sys.stderr, "Failed to exec '%s': %s" % (dbus.dbus_daemon, str(sys.exc_info()[1]))
				sys.stderr.flush()
				os._exit(1)
		os.close(w)
		addr = ''
		while '\n' not in addr:
			try:
				extra = os.read(r, 100)
			except OSError:
				continue
			if not extra:
				if not dbus.dbus_version_ok:
					break
				raise Exception('Failed to read D-BUS address!')
			addr += extra
		if '\n' in addr:
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
		# Don't kill it now, arrange for it to be killed after
		# a short delay.  Why?  If a panel applet uses D-Bus it
		# will exit when the daemon dies and the filer will remove
		# it from the panel.
		os.spawnlp(os.P_NOWAIT, 'sh', 'sh', '-c',
			   'sleep 10; kill %d' % _dbus_pid)
	_dbus_pid = None
