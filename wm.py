import os
from logging import info
import children
import rox
from rox import g
import settings
from utils import shell_split

wm_pid = None
autorestart_wm = False

path_dirs = os.environ.get('PATH', '/bin:/usr/bin').split(':')

window_managers = [
	'0launch http://rox.sourceforge.net/2005/interfaces/OroboROX',
	'xfwm4',
	'sawfish',
	'sawmill',
	'enlightenment',
	'wmaker',
	'icewm',
	'blackbox',
	'fluxbox',
	'metacity',
	'kwin',
	'kwm',
	'fvwm2',
	'fvwm',
	'4Dwm',
	'twm',
	'xterm',
]

def start():
	"""Ensure WM is running."""
	global wm_pid
	assert wm_pid is None

	unused, wm_command = get_window_manager()
	if wm_command:
		wm_pid = os.spawnvp(os.P_NOWAIT, wm_command[0], wm_command)
		children.register_child(wm_pid, wm_died)
		info("Started window manager; PID %s", wm_pid)
		import session
		session.may_run_login_script()
	else:
		choose_wm('None of the default window managers are installed')

def available_in_path(command):
	for x in path_dirs:
		info("Checking for %s/%s", x, command)
		if os.path.isfile(os.path.join(x, command)):
			return True
	return False

def get_window_manager():
	user_wm = settings.settings.get('ROX/WindowManager', None)
	if user_wm:
		wms = [user_wm] + window_managers
	else:
		wms = window_managers
	for wm in wms:
		wm_split = shell_split(wm)
		if available_in_path(wm_split[0]):
			return wm, wm_split
	return None, None

def wm_died(status):
	global wm_pid, autorestart_wm
	wm_pid = None
	if autorestart_wm:
		autorestart_wm = False
		start()
	else:
		choose_wm(_("Your window manager has crashed (or quit). "
			    "Please restart it, or choose another window manager."))

def choose_wm(message):
	box = rox.Dialog('Choose a window manager', None,
		g.DIALOG_MODAL | g.DIALOG_NO_SEPARATOR)
	box.add_button(g.STOCK_CLOSE, g.RESPONSE_CANCEL)
	box.add_button(g.STOCK_EXECUTE, g.RESPONSE_OK)
	box.set_position(g.WIN_POS_CENTER)

	box.vbox.pack_start(g.Label(message), True, True, 0)
	box.set_default_response(g.RESPONSE_OK)

	entry = g.Entry()
	entry.set_activates_default(True)
	box.vbox.pack_start(entry, False, True, 0)
	entry.set_text(get_window_manager()[0] or '')

	box.vbox.show_all()

	if box.run() == int(g.RESPONSE_OK):
		settings.settings.set('ROX/WindowManager',  entry.get_text())
		box.destroy()
		start()
	else:
		box.destroy()

def offer_restart():
	if wm_pid is None: return	# Not yet started
	if rox.confirm(_("Your default window manager is now '%s'.\n"
			"Would you like to quit your current window manager and start the new "
			"one right now?") % get_window_manager()[0],
			g.STOCK_REFRESH, _('Restart')):
		global autorestart_wm
		autorestart_wm = True
		kill_wm()

def kill_wm():
	import signal
	if wm_pid is not None:
		os.kill(wm_pid, signal.SIGTERM)
