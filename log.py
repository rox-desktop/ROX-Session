from rox import g
from rox.options import Option
import time, os, sys
import fcntl
import codecs

import rox

from_utf8 = codecs.getdecoder('utf-8')
from_latin1 = codecs.getdecoder('iso-8859-1')

o_time_shown = Option('log_time_shown', 5)
Option('log_percent_shown', 30)

real_stderr = None

def close_on_exec(fd, close):
	fcntl.fcntl(fd, fcntl.F_SETFD, close)

def init():
	global real_stderr
	log_r, log_w = os.pipe()

	# Since writing to stderr will block until we're ready to
	# read it, it's a good idea to get our own errors some other
	# way (ROX-Session may deadlock due because it can't write the
	# error until it's ready to read previous errors, and can't read
	# previous errors until it's written the new one).
	# Pango likes to spew errors if it can't find its fonts...

	#import warnings
	#def showwarning(message, category, filename, lineno, file = None,
	#		 showwarning = warnings.showwarning):
	#	print category, filename, lineno, message
	#warnings.showwarning = showwarning

	# Grab a copy of stderr before we replace it.
	# We'll duplicate output here if it exists...
	real_stderr = os.dup(2)

	sys.stderr = sys.stdout		# Send errors from us to stdout to avoid deadlock

	# Dup writable stream to stderr
	if log_w != 2:
		os.dup2(log_w, 2)
		os.close(log_w)
	
	close_on_exec(2, False)
	close_on_exec(log_r, True)

	log = Log()
	def got_log_data(src, cond):
		global real_stderr
		got = os.read(src, 1000)
		if not got:
			g.input_remove(input_tag)
			log.log('ROX-Session: read(stderr) failed!\n')
			return False
		try:
			log.log_raw(got)
			while got and real_stderr is not None:
				written = os.write(real_stderr, got)
				if written < 0:
					log.log('ROX-Session: write to stderr failed!')
					os.close(real_stderr)
					real_stderr = None
					break
				got = got[written:]
		except:
			rox.report_exception()
		return True
			
	#os.write(2, 'hi - ' + chr(0xef) + '\n')

	input_tag = g.input_add(log_r, g.gdk.INPUT_READ, got_log_data)

	print "DONE"

class Log:
	raw_input_buffer = ''
	buffer = None		# GtkTextBuffer
	popup = None
	log_window = None
	chunks = []

	def __init__(self):
		self.buffer = g.TextBuffer()
		self.buffer.create_tag('time', foreground = 'blue')

		end = self.buffer.get_end_iter()
		self.buffer.insert(end, "ROX-Session started: ")
		self.buffer.insert_with_tags_by_name(end, time.ctime() + '\n', "time")

	def log_raw(self, data):
		self.raw_input_buffer += data
		while True:
			nl = self.raw_input_buffer.rfind('\n')
			if nl == -1:
				return
			chunk = self.raw_input_buffer[:nl]
			self.raw_input_buffer = self.raw_input_buffer[nl + 1:]
			
			# Got a complete chunk of UTF-8 (no half characters)
			try:
				message = from_utf8(chunk)[0]
			except UnicodeDecodeError:
				message = from_latin1(chunk, 'replace')[0] + ' (invalid UTF-8)'
			self.log(message)

	def log(self, message):
		end = self.buffer.get_end_iter()
		self.buffer.insert_with_tags_by_name(end, time.ctime() + '\n', 'time')
		self.buffer.insert(end, message + '\n')
		
		# TODO: remove stuff from...

		self.chunks.append(Chunk(message, time.time()))
		self.show_log_window()
		#self.show_popup()
	
	def show_popup(self):
		if self.popup is None:
			self.popup = Popup(self.chunks)
		self.popup.show()
	
	def show_log_window(self):
		if self.log_window is None:
			self.log_window = LogWindow(self.buffer)
		self.log_window.show()

class LogWindow(g.Dialog):
	buffer = None

	def __init__(self, buffer):
		g.Dialog.__init__(self)

		self.buffer = buffer
		self.set_title(_("ROX-Session message log"))
		#self.set_name('log_window')
		self.set_has_separator(False)

		self.add_button(g.STOCK_CLOSE, g.RESPONSE_OK)

		self.tv = g.TextView(buffer)
		self.tv.set_size_request(400, 100)
		self.tv.set_editable(False)
		self.tv.set_cursor_visible(False)

		swin = g.ScrolledWindow(None, None)
		swin.set_policy(g.POLICY_AUTOMATIC, g.POLICY_AUTOMATIC)
		swin.set_shadow_type(g.SHADOW_IN)
		swin.add(self.tv)

		swin.show_all()
		self.vbox.pack_start(swin, True, True, 0)

		screen = g.gdk.screen_get_default()
		mx, my, mask = g.gdk.get_default_root_window().get_pointer()
		monitor = screen.get_monitor_at_point(mx, my)
		geometry = screen.get_monitor_geometry(monitor)
		self.set_default_size(geometry.width / 2, geometry.height / 4)

		def resp(box, r):
			self.hide()
		self.connect('response', resp)

		def delete(box, dev):
			return True
		self.connect('delete-event', delete)
	
	def show(self):
		cursor = self.buffer.get_mark('insert')
		self.tv.scroll_to_mark(cursor, 0)
		g.Dialog.show(self)
	
	def clicked(self, win, bev):
		print bev

class Popup(g.Window):
	def __init__(self, buffer):
		g.Window.__init__(self, g.WINDOW_POPUP)
		#self.set_resizable(False)
		self.set_name('log_window')
		self.realize()
		self.add_events(g.gdk.BUTTON_PRESS_MASK)
		self.connect('button-press-event', self.clicked)

		tv = g.TextView(buffer)
		self.add(tv)
		#label.add_events(g.gdk.BUTTON_PRESS_MASK)
		tv.show()
	
	def clicked(self, win, bev):
		print bev
	
	def display(self):
		if self.message_window or not self.chunks or not o_time_shown.int_value:
			# We have the scrolling window open, there is nothing
			# to show, or no time to show it. Hide the popup window.
			self.hide()
			return

		screen = g.gdk.screen_get_default()

class Chunk:
	def __init__(self, message, timestamp):
		self.message = message
		self.timestamp = timestamp
