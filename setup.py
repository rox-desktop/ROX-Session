import rox
from rox import g, processes
import os

def add_button(dialog, stock_icon, action, response):
	button = rox.ButtonMixed(stock_icon, action)
	button.set_flags(g.CAN_DEFAULT)
	button.show()
	dialog.add_action_widget(button, response)

def test_rox_session():
	apprun = os.path.join(rox.app_dir, 'AppRun')
	proc = processes.PipeThroughCommand([apprun, '-v'], None, None)
	proc.wait()

def backup(old, backup):
	"""Move 'old' as 'backup'"""
	if os.path.islink(old):
		os.unlink(old)	# Don't backup symlinks
		return
	if not os.path.exists(old):
		return		# Nothing to backup
	if os.path.exists(backup):
		rox.croak(_("Tried to make a backup of your old '%s' file, "
			"but the backup file ('%s') already exists.") %
			(old, backup))
	os.rename(old, backup)

def setup():
	test_rox_session()

	box = g.MessageDialog(None, g.DIALOG_MODAL,
			g.MESSAGE_QUESTION,
			g.BUTTONS_CANCEL,
			_('Do you want to make ROX a choice '
			'on your login screen (affects all users and '
			'requires the root password), or do you '
			'just want to set the session for your current '
			'user?\n\n'
			'If you know the root password and use '
			'a graphical display manager such as gdm or kdm, '
			'you should probably choose that option.'))
	add_button(box, g.STOCK_HOME, _('Setup for user'), 1)
	add_button(box, g.STOCK_YES, _('Add to login'), 2)

	box.set_default_response(2)

	box.set_position(g.WIN_POS_CENTER)
	resp = box.run()
	box.destroy()
	if resp == 1:
		setup_home()
	elif resp == 2:
		setup_login()

def create_session_script(path):
	"""Create login script at 'path' and make it executable."""
	if os.path.exists(path):
		if not rox.confirm("File '%s' already exists; overwrite?" % path):
			raise SystemExit()
	file(path, 'w').write("""#!/bin/sh
# This file was created by ROX-Session.

if [ -d "$HOME/bin" ]; then
	PATH="${HOME}/bin:${PATH}"
	export PATH
fi

# Step 1: Try to run ROX-Session. If it works, stop right here.

if [ -x "%s/AppRun" ]; then
	exec "%s/AppRun" -w
fi

# Step 2: It didn't work. Try to provide a failsafe login so the user
# can fix things.

# Load a window manager. Keep trying until we find one that works!

for wm in xfwm4 sawfish sawmill enlightenment wmaker icewm blackbox fluxbox \\
	metacity kwin kwm fvwm2 fvwm 4Dwm twm; do
  if [ -x "`which $wm`" ]; then break; fi;
done

"$wm" &

xmessage -file - << END
.xsession: failed to run %s/AppRun - maybe you moved or deleted it?

I'll try to give you an xterm and a filer window instead - try to find
and run ROX-Session to fix the problem. Close the xterm to logout.

If all else fails, delete your .xsession and .xinitrc files to get the
system defaults.

Report any problems to:
http://rox.sourceforge.net/phpwiki/index.php/MailingLists

Good luck!
END
rox &
exec xterm
""" % (rox.app_dir, rox.app_dir, rox.app_dir))
	os.chmod(path, 0755)

def setup_home():
	xsession = os.path.expanduser('~/.xsession')
	xinitrc = os.path.expanduser('~/.xinitrc')

	backup(xsession, os.path.expanduser('~/xsession.old'))
	backup(xinitrc, os.path.expanduser('~/xinitrc.old'))

	create_session_script(xsession)
	os.symlink(xsession, xinitrc)
	rox.info(_("OK, now logout by your usual method and when "
		"you log in again, I should be your session manager.\n"
		"Note: you may need to select 'Default' as your "
		"desktop type after entering your user name on the "
		"login screen."))

def setup_login():
	session_dirs = ['/etc/X11/sessions', '/etc/dm/Sessions',
			'/etc/X11/dm/Sessions', '/usr/share/xsessions']
	for d in session_dirs:
		if os.path.isdir(d):
			session_dir = d
			break
	else:
		rox.croak(_('I wanted to install a rox.desktop file in your '
			"session directory, but I couldn't find one! I tried "
			"these places (defaults for gdm2, at least):\n\n") +
			'\n'.join(session_dirs))
	if not os.path.isdir('/usr/local/sbin'):
		rox.croak(_('/usr/local/sbin directory is missing! I want to '
			'install the rox-session script there... Please create it '
			'and try again.'))
	import tempfile
	desktop = tempfile.mktemp('-rox-session')
	file(desktop, 'w').write("""[Desktop Entry]\n
Encoding=UTF-8
Name=ROX
Comment=This session logs you into the ROX desktop
Exec=/usr/local/sbin/rox-session
Type=Application
""")
	script = tempfile.mktemp('-rox-session')
	create_session_script(script)
	install_as_root(desktop, os.path.join(session_dir, 'rox.desktop'),
			 script, '/usr/local/sbin/rox-session')
	os.unlink(desktop)
	os.unlink(script)
	rox.info(_("OK, now logout by your usual method, and choose ROX from "
		"the session menu on your login screen just after entering your "
		"user name (but before entering your password)."))

def troubleshoot():
	if not rox.confirm("Did you select 'ROX' from the login screen after "
		'entering your username, but before entering your '
		'password?\n\n'
		"(if you installed to your home directory, you should have "
		"chosen an option named 'default' or 'last session')",
			g.STOCK_YES):
		return
		
	rox.croak(_("OK, I don't know what the problem is. Please ask on the "
		"rox-users mailing list. When we know what the problem is, we "
		"can add an extra check here to help out others in future."
		"\n\n"
		"http://rox.sourceforge.net/phpwiki/index.php/MailingLists\n"
		"(you can copy and paste that address into your browser)"))

# Used when the user just clicked directly on ROX-Session
def setup_with_confirm():
	box = g.MessageDialog(None, 0, g.MESSAGE_QUESTION,
				g.BUTTONS_CANCEL,
		_('ROX-Session does not appear to be managing '
		'your session. Would you like to set it up now?\n\n'
		'If you think it should already be set up, click on '
		'Help.'))
	box.add_button(g.STOCK_HELP, g.RESPONSE_HELP)
	add_button(box, g.STOCK_YES, _('Set up ROX'), g.RESPONSE_OK)

	box.set_position(g.WIN_POS_CENTER)
	box.set_title(_('Set up ROX-Session'))
	box.set_default_response(g.RESPONSE_OK)

	resp = box.run()
	box.destroy()

	if resp == g.RESPONSE_OK:
		setup()
	elif resp == g.RESPONSE_HELP:
		troubleshoot()

def install_as_root(d_src, d_dest, s_src, s_dest):
	rox.info(_('Please enter the root password when prompted. I will '
		"attempt to create the files '%s' and '%s'.") % (d_dest, s_dest))
	os.environ['SRC1'] = s_src
	os.environ['DST1'] = s_dest
	os.environ['SRC2'] = d_src
	os.environ['DST2'] = d_dest
	if os.spawnlp(os.P_WAIT, 'xterm', 'xterm', '-e', 'su', '-c',
			os.path.join(rox.app_dir, 'install-root.sh')):
		rox.croak(_("Error trying to run xterm to ask for the root password."))
	if not os.path.exists(d_dest):
		rox.croak(_("Failed to create '%s'") % d_dest)
	if not os.path.exists(s_dest):
		rox.croak(_("Failed to create '%s'") % s_dest)
	#rox.info('Please copy %s as %s. Ta' % (src, dest))
