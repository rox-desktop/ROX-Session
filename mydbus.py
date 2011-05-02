import sys, os
from logging import info

dbus_version_ok = False
version = (0, 0, 0)
min_version = (0, 42, 0)
try:
	from dbus import *
	from dbus import version  # Package contents changed in 0.80
	if version >= min_version:
		dbus_version_ok = True
		info("D-BUS 0.3x detected. OK.")
	else:
		dbus_version_ok = False
		info("D-BUS too old. No D-BUS support.")
except ImportError:
	# Don't really care, now we have XML-RPC support.
	dbus_version_ok = False
	info("Failed to import dbus. No D-BUS support.")

# Try to start the session bus anyway...
dbus_daemon = 'dbus-daemon'
