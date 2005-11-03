from rox import g
from rox.options import Option
import time

Option('log_time_shown', 5)
Option('log_percent_shown', 30)

def init():
	buffer = g.TextBuffer()
	buffer.create_tag('time', 'foreground', 'blue')

	end = buffer.get_end_iter()
	buffer.insert(end, "ROX-Session started: ")
	buffer.insert_with_tags_by_name(end, time.ctime(), "time")

	log_r, log_w = os.pipe()
