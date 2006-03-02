import mydbus as dbus
import xsettings

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

if dbus.dbus_version == 2:
	from settings2x import *
elif dbus.dbus_version is None:
	def real_init(manager): pass
elif dbus.version < (0, 42, 0):
	# XXX: when did the API break for service in the Python bindings?
	# The NEWS file implies it was 0.35 which is (0,42,0)
	from settings2x import *
else:
	from settings3x import *

settings = None
def init():
	global settings
	settings = XMLSettings()
	real_init(settings.xsettings_manager)
	return settings

def destroy():
	print "TODO: destroy"
