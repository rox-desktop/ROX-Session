import xsettings
import sys

class XMLSettings:
	allowed_methods = ('GetSetting', 'SetInt', 'SetString')

	def __init__(self):
		self.xsettings_manager = xsettings.Manager(0)

	def SetString(self, key, value):
		self.set(key, str(value))
	
	def SetInt(self, key, value):
		self.set(key, int(value))

	def set(self, key, value):
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

	def get(self, key, default):
		return self.xsettings_manager.get(key, default)

settings = None
def init():
	global settings
	settings = XMLSettings()

	import mydbus
	if mydbus.dbus_version_ok:
		import settings3x
		settings3x.real_init(settings.xsettings_manager)

	return settings
