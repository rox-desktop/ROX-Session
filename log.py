from rox import g
from rox.options import Option
import time, os, sys, gobject
import fcntl
import codecs

import rox

from_utf8 = codecs.getdecoder('utf-8')
from_latin1 = codecs.getdecoder('iso-8859-1')

o_time_shown = Option('log_time_shown', 5)
o_percent_switch = Option('log_percent_switch', 30)

MARGIN = 4

real_stderr = None

MAX_LOG_SIZE = 100000	# If complete message log exceeds this many characters, trim it (100K)

def close_on_exec(fd, close):
	fcntl.fcntl(fd, fcntl.F_SETFD, close)

def init():
	global real_stderr, log
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

class Log(object):
	raw_input_buffer = ''
	buffer = None		# GtkTextBuffer
	popup = None
	log_window = None
	chunks = []
	chunks_cleanup = None
	last_timestamp_logged = None

	def __init__(self):
		self.buffer = g.TextBuffer()
		self.buffer.create_tag('time', foreground = 'blue')

		self.last_timestamp_logged = time.time()

		end = self.buffer.get_end_iter()
		self.buffer.insert(end, "ROX-Session started: ")
		now = time.ctime(self.last_timestamp_logged)
		self.buffer.insert_with_tags_by_name(end, now + '\n', "time")

	def log_raw(self, data):
		self.raw_input_buffer += data
		try:
			message = from_utf8(self.raw_input_buffer)[0]
			self.raw_input_buffer = ''
		except UnicodeDecodeError:
			# Not valid UTF-8. Maybe we're part way through a
			# multi-byte sequence. Just go up to newline.
			nl = self.raw_input_buffer.rfind('\n')
			if nl == -1:
				return	# No newline. Wait for more input.

			chunk = self.raw_input_buffer[:nl]
			self.raw_input_buffer = self.raw_input_buffer[nl + 1:]
			
			# Got a complete chunk of UTF-8 (no half characters)
			try:
				message = from_utf8(chunk)[0]
			except UnicodeDecodeError:
				# Still not valid. Try latin1, so we can print something.
				message = from_latin1(chunk, 'replace')[0] + ' (invalid UTF-8)'
		self.log(message)

	def log(self, message):
		# Remove blank lines and trailing spaces
		lines = [line.lstrip() for line in message.split('\n')]
		message = '\n'.join(filter(None, lines))

		end = self.buffer.get_end_iter()

		# Log the time, but only if it hasn't been too long (30
		# seconds) since the last timestamp.
		now = time.time()
		if now > self.last_timestamp_logged + 30:
			self.buffer.insert_with_tags_by_name(end, time.ctime(now) + '\n', 'time')
			self.last_timestamp_logged = now
		self.buffer.insert(end, message + '\n')
		
		self.prune()

		if self.log_window and self.log_window.flags() & g.VISIBLE:
			# Full log window already open
			self.show_log_window()
		else:
			# Otherwise try the popup
			self.chunks.append(Chunk(message, time.time()))
			self.schedule_chunks_cleanup()
			self.show_popup()
	
	def prune(self):
		"""If self.buffer is too long, remove lines from the start."""
		chars = self.buffer.get_char_count()
		if chars <= MAX_LOG_SIZE:
			return
		start = self.buffer.get_start_iter()
		end = self.buffer.get_start_iter()
		end.forward_chars(chars - MAX_LOG_SIZE)
		end.forward_line()		# Remove complete lines only
		self.buffer.delete(start, end)
	
	def show_popup(self):
		if self.popup is None:
			self.popup = Popup(self)
		self.popup.show()
	
	def show_log_window(self):
		if self.popup and self.popup.flags() & g.VISIBLE:
			self.popup.hide()
		if self.log_window is None:
			self.log_window = LogWindow(self.buffer)
		self.chunks = []
		self.log_window.show()
	
	def schedule_chunks_cleanup(self):
		if self.chunks_cleanup is not None:
			gobject.source_remove(self.chunks_cleanup)
			self.chunks_cleanup = None
		self.expire_chunks()
		if not self.chunks:
			return
		delay = self.chunks[0].timestamp + o_time_shown.int_value - time.time()
		def expire():
			self.chunks_cleanup = None
			self.schedule_chunks_cleanup()
			return False
		self.chunks_cleanup = gobject.timeout_add(int(max(delay * 1000, 0)) + 1, expire)
	
	def expire_chunks(self):
		earliest_to_keep = time.time() - o_time_shown.int_value
		changed = False
		while self.chunks and self.chunks[0].timestamp < earliest_to_keep:
			del self.chunks[0]
			changed = True
		if changed:
			if self.chunks:
				self.show_popup()
			elif self.popup:
				self.popup.hide()

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
	
class Popup(g.Window):
	def __init__(self, log):
		g.Window.__init__(self, g.WINDOW_POPUP)
		#self.set_resizable(False)
		self.set_name('log_window')
		self.realize()
		self.add_events(g.gdk.BUTTON_PRESS_MASK)
		self.connect('button-press-event', self.clicked)

		self.log = log
		self.label = g.Label(buffer)
		self.label.set_alignment(0.0, 0.0)
		self.add(self.label)
		#label.add_events(g.gdk.BUTTON_PRESS_MASK)
		self.label.show()
	
	def clicked(self, win, bev):
		self.hide()
		if bev.button != 1:
			self.log.show_log_window()
	
	def show(self):
		message = ''
		for c in self.log.chunks:
			message += c.message
		# Remove blank lines
		message = '\n'.join([m for m in message.split('\n') if m.strip()])
		self.label.set_text(message)

		screen = g.gdk.screen_get_default()
		mx, my, mask = g.gdk.get_default_root_window().get_pointer()
		monitor = screen.get_monitor_at_point(mx, my)
		geometry = screen.get_monitor_geometry(monitor)

		req_width, req_height = self.label.size_request()
		max_h = o_percent_switch.int_value * geometry.height / 100

		top = 2 * my > geometry.height + geometry.y

		if req_height > max_h:
			self.hide()
			self.log.show_log_window()
			return

		if top:
			y = geometry.y + MARGIN
		else:
			y = geometry.y + geometry.height - MARGIN - req_height

		req_width = geometry.width - 2 * MARGIN

		#self.set_uposition(geometry.x + MARGIN, y)
		self.set_size_request(req_width, req_height)
		g.Window.show(self)
		self.window.move_resize(MARGIN, y, req_width, req_height)
		self.window.raise_()

class Chunk:
	def __init__(self, message, timestamp):
		self.message = message
		self.timestamp = timestamp
