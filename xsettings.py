import os, sys
from logging import info
from xml.dom import Node, minidom

import rox
from rox import g, basedir
import constants

def manager_check_running(screen):
	return _get_manager(screen) is not None

def _property_name(screen):
	return "_XSETTINGS_S%d" % screen

def _get_manager(screen):
	return g.gdk.selection_owner_get(g.gdk.atom_intern(_property_name(screen)))

class XSetting:
	def __init__(self, value, serial = 0):
		self.value = value
		self.serial = serial

	def serialise(self, name):
		return (self.type + '\0' +
			intToLittleEndian16(len(name)) +
			name + padding(len(name)) +
			intToLittleEndian32(self.serial) +
			self.serialise_body())
	
	def __repr__(self):
		return "<%s>" % self.value

class IntXSetting(XSetting):
	type = '\0' 	# XSETTINGS_TYPE_INT 
	str_type = 'int'

	def serialise_body(self):
		return intToLittleEndian32(self.value)

class StrXSetting(XSetting):
	type = '\1' 	# XSETTINGS_TYPE_STRING
	str_type = 'string'

	def serialise_body(self):
		return (intToLittleEndian32(len(self.value)) + 
			self.value + padding(len(self.value)))

class Manager:
	timestamp = None
	option_group = None

	def __init__(self, screen):
		self.screen = screen

		self.selection_atom = g.gdk.atom_intern(_property_name(screen), False)
		self.xsettings_atom = g.gdk.atom_intern('_XSETTINGS_SETTINGS', False)
		self.manager_atom = g.gdk.atom_intern('MANAGER', False)

		self.serial = 0

		self.window = g.Invisible()
		self.window.add_events(g.gdk.PROPERTY_CHANGE_MASK)
		self.window.connect('property-notify-event', self.property_notify)

		if manager_check_running(0) and False:	# XXX
			print >>sys.stderr, _("An XSETTINGS manager is already running. "
					"Not taking control of XSETTINGS...")
			return
		else:
			g.gdk.selection_owner_set(self.window.window, self.selection_atom,
					self.get_server_time(), False)

			if _get_manager(screen) != self.window.window:
				info('Failed to acquire XSettings manager selection')
				self.terminate()
				return

		# Can't see how to do this with PyGTK. But, since nothing else is
		# running at this point, we're probably OK.

		# XSendEvent (display, RootWindow (display, screen),
		#		False, StructureNotifyMask, (XEvent *)&xev);
		info('Acquired XSettings selection successfully - window %s', self.window.window)

		# Load settings
		try:
			path = basedir.load_first_config(constants.site, 'ROX-Session', 'Settings.xml')
			if path:
				self.load_settings(path)
		except:
			rox.report_exception()

		self.notify()
	
	def load_settings(self, path):
		doc = minidom.parse(path)
		
		root = doc.documentElement
		assert root.localName == 'Settings'
		for o in root.childNodes:
			if o.nodeType != Node.ELEMENT_NODE:
				continue
			if o.localName == 'Setting':
				name = str(o.getAttribute('name'))
				value = o.getAttribute('value')
				type = o.getAttribute('type')
				print name, type, value
				if type == 'string':
					self.set(name, str(value))
				elif type == 'int':
					self.set(name, int(value))
				else:
					raise Exception('Unknown type "%s"' % type)
			else:
				print "Warning: Non Option element", o

	def get_server_time(self):
		assert self.timestamp is None

		atom = g.gdk.atom_intern('_TIMESTAMP_PROP', False)
		self.window.window.property_change(atom, atom,
						8, g.gdk.PROP_MODE_REPLACE, 'a')
		while self.timestamp is None:
			info("Waiting for timestamp...")
			g.main_iteration()
		time = self.timestamp
		self.timestamp = None
		return long(time)

	def property_notify(self, window, event):
		self.timestamp = event.time
	
	def terminate(self):
		print "Terminate"
		return
	
	def set(self, name, value):
		old = self._settings.get(name, None)
		if old is not None:
			if old.value == value: return	# No change
		if isinstance(value, str):
			new = StrXSetting(value, self.serial)
		elif isinstance(value, int):
			new = IntXSetting(int(value), self.serial)
		else:
			raise Exception('Unknown type for "%s"' % `value`)
		self._settings[name] = new
	
	def get(self, name, default):
		setting = self._settings.get(name, None)
		#print "Value for", name, "is", setting
		if setting is None:
			return default
		return setting.value
	
	def notify(self):
		"""Push settings to other apps"""
		# Note: we always use little-endian values
		buffer = ('\0\0\0\0' +
			 intToLittleEndian32(self.serial) +
			 intToLittleEndian32(len(self._settings)))
		for s in self._settings:
			buffer += self._settings[s].serialise(s)

		self.serial += 1

		#print "Set\t", `buffer`
			 
		self.window.window.property_change(self.xsettings_atom,
						   self.xsettings_atom, 8,
						   g.gdk.PROP_MODE_REPLACE,
						   buffer)

	def save(self):
		doc = minidom.parseString("<Settings/>")
		root = doc.documentElement
		for x in self._settings:
			elem = doc.createElement('Setting')
			root.appendChild(elem)
			elem.setAttribute('name', x)
			setting = self._settings[x]
			elem.setAttribute('value', str(setting.value))
			elem.setAttribute('type', str(setting.str_type))
		save_dir = basedir.save_config_path(constants.site, 'ROX-Session')
		settings_file = os.path.join(save_dir, 'Settings.xml')
		stream = file(settings_file + '.new', 'w')
		doc.writexml(stream, addindent = ' ', newl = '\n')
		stream.close()
		os.rename(settings_file + '.new', settings_file)

	_settings = {
		'Gtk/FontName': StrXSetting('Sans 10'),
		'Net/ThemeName': StrXSetting('Default'),
		'Net/CursorBlinkTime': IntXSetting(1200),
		'Xft/Antialias': IntXSetting(1),

		'ROX/DPMSEnable': IntXSetting(1),
		'ROX/DPMSStandby': IntXSetting(15 * 60),
		'ROX/DPMSSuspend': IntXSetting(20 * 60),
		'ROX/DPMSOff': IntXSetting(30 * 60),

		'mouse_accel_factor': IntXSetting(20),
		'mouse_accel_threshold': IntXSetting(10),
		'mouse_left_handed': IntXSetting(0),

		'cursor_theme': StrXSetting(''),
		'cursor_size': IntXSetting(18),

		'kbd_repeat': IntXSetting(1),
		'kbd_numlock': IntXSetting(0),
		'kbd_capslock': IntXSetting(0),
		'kbd_delay': IntXSetting(500),
		'kbd_interval': IntXSetting(30),

		'manage_screensaver': IntXSetting(0),
		'blank_time': IntXSetting(10 * 60),
	}

def padding(length):
	needed = (length + 3) & ~3
	return '\0' * (needed - length)

def intToLittleEndian16(i):
	return chr(i & 0xff) + chr((i >> 8) & 0xff)

def intToLittleEndian32(i):
	return chr(i & 0xff) + chr((i >> 8) & 0xff) + chr((i >> 16) & 0xff) + chr((i >> 24) & 0xff)
