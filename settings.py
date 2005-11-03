from logging import info
import sys

import constants
import dbus
import xsettings

settings = None

class Settings(dbus.Object):
	def __init__(self, service):
		dbus.Object.__init__(self, "/Settings", service, [
			self.GetSetting,
			self.SetInt,
			self.SetString])
		self.xsettings_manager = xsettings.Manager(0)

	def SetString(self, msg, key, value):
		self.set(key, str(value))
	
	def SetInt(self, msg, key, value):
		self.set(key, int(value))

	def set(self, key, value):
		info("Setting %s = %s", key, value)
		self.xsettings_manager.set(key, value)
		self.xsettings_manager.notify()
		self.xsettings_manager.save()
	
	def GetSetting(self, msg, key):
		try:
			value = self.get(key, default = None)
			if value is not None:
				if isinstance(value, str):
					return ["string", value]
				else:
					return ["int", str(value)]
			raise Exception('Missing setting')
		except Exception, ex:
			print >>sys.stderr, ex
			raise ex

	def get(self, key, default):
		return self.xsettings_manager.get(key, default)

def init():
	global settings
	session_bus = dbus.SessionBus()
	service = dbus.Service(constants.session_service, bus = session_bus)
	settings = Settings(service)

def destroy():
	print "destroy settings"
