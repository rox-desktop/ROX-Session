import sys, os
from logging import info

dbus_version_ok = False
version = (0, 0, 0)
try:
	from dbus import *
	if version > (0, 42, 0):
		dbus_version_ok = True
		dbus_daemon = 'dbus-daemon'
		info("D-BUS 0.3x detected. OK.")
	else:
		dbus_version_ok = False
		info("D-BUS too old. No D-BUS support.")
except ImportError:
	# Don't really care, now we have XML-RPC support.
	dbus_version_ok = False
	info("Failed to import dbus. No D-BUS support.")
