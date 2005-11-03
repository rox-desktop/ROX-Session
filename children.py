import os
import gobject
from logging import info
import signal

child_died_callbacks = {}	# PID -> Function

def register_child(pid, died_callback):
	assert pid not in child_died_callbacks
	child_died_callbacks[pid] = died_callback

def child_died(sig, frame):
	info("Got child_died signal... scheduling callback")
	gobject.idle_add(child_died_callback)

def child_died_callback():
	# This is called from the mainloop when idle, after one or more child
	# processes have died. The reason for waiting is that there is a window
	# between spawning a child and assigning its PID to a variable, during which
	# the signal handler may get called if the child exits very quickly.
	info("child_died_callback")
	while True:
		try:
			pid, status = os.waitpid(-1, os.WNOHANG)
			if pid == 0:
				info("No zombie child processes")
				return
		except OSError:
			info("No more processes to reap")
			return
		info("Child %s died with exit status %s", pid, status)
		fn = child_died_callbacks.get(pid, None)
		if fn:
			del child_died_callbacks[pid]
			info("Calling handler %s", fn)
			fn(status)

def init():
	# Let child processes die
	signal.signal(signal.SIGCHLD, child_died)
