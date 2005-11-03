import sys

import rox
from rox import g
import os.path
import constants

import dbus_compat
sys.modules['dbus.services'] = dbus_compat

try:
	import dbus
	if hasattr(dbus, 'version') and dbus.version >= (0, 40, 0):
		DBusException = dbus.DBusException
	else:
		DBusException = dbus.dbus_bindings.DBusException
	# Else, using D-BUS 0.2
except ImportError:
	rox.alert("Failed to import dbus module. You probably need "
		"to install a package with a name like 'python2.3-dbus'.\n\n"
		"D-BUS can also be downloaded from http://freedesktop.org\n"
		"(be sure to compile the python and glib bindings)\n\n"
		"If the bindings are installed in /usr/local/lib/python...,"
		"try moving them to /usr/lib/python... (without the 'local')")
	raise

try:
	bus = dbus.Bus(dbus.Bus.TYPE_SESSION)
	if hasattr(bus, 'get_service'):
		dbus_service = bus.get_service('org.freedesktop.DBus')
		dbus_object = dbus_service.get_object('/org/freedesktop/DBus',
						   'org.freedesktop.DBus')
		dbus_services = dbus_object.ListServices()
	else:
		dbus_object = bus.get_object('org.freedesktop.DBus',
					    '/org/freedesktop/DBus')
		dbus_services = dbus_object.ListNames()
	rox_session_running = constants.session_service in dbus_services
except DBusException, ex:
	print ex
	rox_session_running = False
except:
	rox.report_exception()
	raise SystemExit()

try:
	if rox_session_running:
		import logout
		if hasattr(bus, 'get_service'):
			rox_session = bus.get_service(constants.session_service)
			session_control = rox_session.get_object('/Session',
						'net.sf.rox.Session.Control')
		else:
			session_control = dbus.Interface(
				bus.get_object('net.sf.rox.Session', '/Session'),
				'net.sf.rox.Session.Control')

		logout.show_logout_box(session_control)
	else:
		import setup
		setup.setup_with_confirm()
except SystemExit:
	pass
except:
	rox.report_exception()
