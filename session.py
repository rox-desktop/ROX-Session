import signal, os
from logging import info
import gobject

import rox
from rox import g, basedir
import constants
import children

stocks = ['rox-halt', 'rox-suspend']

o_halt = rox.options.Option('halt_command', 'halt')
o_reboot = rox.options.Option('reboot_command', 'reboot')
o_suspend = rox.options.Option('suspend_command', 'xset dpms force off')

def init():
	factory = g.IconFactory()
	for name in stocks:
		path = os.path.join(rox.app_dir, "images", name + ".png")
		info("Loading image %s", path)
		pixbuf = g.gdk.pixbuf_new_from_file(path)

		iset = g.IconSet(pixbuf)
		factory.add(name, iset)
	factory.add_default()

_logged_in = False

def may_run_login_script():
	"""Called once the WM is running."""
	global _logged_in
	global login_child

	if _logged_in:
		return

	_logged_in = True

	# Run ROX-Filer
	run_rox_process()

	# Run Login script

	login = basedir.load_first_config(constants.site, 'ROX-Session', 'Login') or \
		os.path.join(rox.app_dir, 'Login')

	login_child = os.spawnlp(os.P_NOWAIT, login, login)

	def login_died(status):
		global login_child
		login_child = None
		if status != 0:
			rox.alert(_("Your login script ('%s') failed. "
				"I'll give you an xterm to try and fix it. ROX-Session "
				"itself is running fine though - run me a second time "
				"to logout."))
			os.spawnlp(os.P_NOWAIT, 'xterm', 'xterm')

	children.register_child(login_child, login_died)

def run_rox_process():
	global rox_pid
	run_rox = basedir.load_first_config(constants.site, 'ROX-Session', 'RunROX') or \
		os.path.join(rox.app_dir, 'RunROX')
	try:
		rox_pid = os.spawnlp(os.P_NOWAIT, run_rox, run_rox, rox.app_dir)
		children.register_child(rox_pid, rox_process_died)
	except:
		rox.report_exception()
		rox_process_died(0)

def rox_process_died(status):
	global rox_pid
	rox_pid = None

	box = g.MessageDialog(parent = None, flags = 0, type = g.MESSAGE_QUESTION,
			buttons = 0,
			message_format = _("ROX-Filer has terminated (crashed?)."
					   "You should probably try to restart it."))

	for stock, label, response in [
			(g.STOCK_NO, _("Do nothing"), 0),
                        (g.STOCK_EXECUTE, _("Run Xterm"), 1),
                        (g.STOCK_REFRESH, _("_Restart"), 2)]:
		button = rox.ButtonMixed(stock, label)
		button.set_flags(g.CAN_DEFAULT)
		box.add_action_widget(button, response)
		button.show()
	
	box.set_default_response(2)

	r = box.run()
	box.destroy()

	if r == 2:
		run_rox_process()
	elif r == 1:
		os.spawnlp(os.P_NOWAIT, 'xterm', 'xterm')
