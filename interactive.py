#!/usr/bin/env python
import sys

import findrox; findrox.version(1, 9, 16)
import rox
from rox import g
import os.path

__builtins__._ = rox.i18n.translation(os.path.join(rox.app_dir, 'Messages'))

if len(sys.argv) == 2 and sys.argv[1] == '--install':
	import setup
	setup.setup()
	raise SystemExit()

try:
	import dbus
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
	dbus_service = bus.get_service('org.freedesktop.DBus')
	dbus_object = dbus_service.get_object('/org/freedesktop/DBus',
						   'org.freedesktop.DBus')
	rox_session_running = 'net.sf.rox.Session' in dbus_object.ListServices()
except dbus.dbus_bindings.DBusException:
	rox_session_running = False
except:
	rox.report_exception()
	raise SystemExit()

try:
	if rox_session_running:
		import logout
		rox_session = bus.get_service('net.sf.rox.Session')
		logout.show_logout_box(rox_session)
	else:
		import setup
		setup.setup_with_confirm()
except SystemExit:
	pass
except:
	rox.report_exception()
