from logging import info
import sys

import constants
import mydbus as dbus
import xsettings

class Settings(dbus.Object):
	def __init__(self, service, manager):
		dbus.Object.__init__(self, "/Settings", service)
		self.xsettings_manager = manager

	def SetString(self, key, value):
		self.set(key, str(value))
	SetString=dbus.method(constants.settings_interface)(SetString)
	
	def SetInt(self, key, value):
		self.set(key, int(value))
	SetInt=dbus.method(constants.settings_interface)(SetInt)

	def set(self, key, value):
		info("Setting %s = %s", key, value)
		self.xsettings_manager.set(key, value)
		self.xsettings_manager.notify()
		self.xsettings_manager.save()
	
	def GetSetting(self, key):
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
	GetSetting=dbus.method(constants.settings_interface)(GetSetting)


	def get(self, key, default):
		return self.xsettings_manager.get(key, default)

def real_init(manager):
	session_bus = dbus.SessionBus()
	service = dbus.Service(constants.session_service,
				       bus = session_bus)
	settings = Settings(service, manager)
	info('now settings=%s', settings)
	return settings

def destroy():
	info("destroy settings")
