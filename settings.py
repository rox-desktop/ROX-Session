import dbus
if hasattr(dbus, 'version') and dbus.version >= (0, 40, 0):
	dbus_version = 3
else:
	dbus_version = 2

if dbus_version == 2:
	from settings2x import *
else:
	from settings3x import *

settings = None
def init():
	global settings
	
	settings = real_init()

print 'settings.settings=', settings
