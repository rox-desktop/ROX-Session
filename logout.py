import rox
from rox import g
import os

# Load icons
factory = g.IconFactory()
for name in ['rox-suspend', 'rox-halt']:
	path = os.path.join(rox.app_dir, "images", name + ".png")
	pixbuf = g.gdk.pixbuf_new_from_file(path)
	if not pixbuf:
		print >>sys.stderr, "Can't load stock icon '%s'" % name
	g.stock_add([(name, name, 0, 0, "")])
	factory.add(name, g.IconSet(pixbuf = pixbuf))
factory.add_default()

def op_button(text, stock, command, message):
	button = g.Button()

	hbox = g.HBox(False, 0)
	button.add(hbox)

	image = g.Image()
	image.set_from_stock(stock, g.ICON_SIZE_BUTTON)
	hbox.pack_start(image, False, True, 4)

	label = g.Label()
	label.set_text_with_mnemonic(text)
	label.set_alignment(0, 0.5)
	hbox.pack_start(label, True, True, 0)

	# No need for clickable buttons if no command is set
	tmp = command.strip()
	if tmp:
		def invoke(button):
			rox.info(message)
			#os.system(command)
		button.connect('clicked', invoke)
	else:
		button.set_sensitive(False)

	button.unset_flags(g.CAN_FOCUS)
	return button

class LogoutBox(rox.Dialog):
	def __init__(self):
		rox.Dialog.__init__(self)
		# TODO: Check if there's already a logout box...
	
		self.set_has_separator(False)
		vbox = self.vbox

		hbox = g.HButtonBox()
		hbox.set_border_width(2)
		hbox.set_layout(g.BUTTONBOX_END)
		vbox.pack_start(hbox, False, True, 0)

		reboot_command = ""
		halt_command = ""
		suspend_command = ""

		button = op_button(_('_Halt'), 'rox-halt',
			halt_command,
			_('Attempting to halt the system...'))
		hbox.pack_end(button, False, True, 0)

		button = op_button(_('_Reboot'), g.STOCK_REFRESH,
			reboot_command,
			_('Attempting to restart the system...'))
		hbox.pack_end(button, False, True, 0)
		
		button = op_button(_('_Sleep'), 'rox-suspend',
			suspend_command,
			_('Attempting to enter suspend mode...'))
		hbox.pack_end(button, False, True, 0)

		vbox.pack_start(g.Label(
			_('Really logout?\n(unsaved data will be lost)')),
			True, True, 20)

		vbox.show_all()

		self.set_title(_('ROX-Session'))

		self.set_position(g.WIN_POS_CENTER)

		button = rox.ButtonMixed(g.STOCK_PREFERENCES,
					_("Session Settings"))
		button.show()
		self.add_action_widget(button, 1)

		self.add_button(g.STOCK_CANCEL, g.RESPONSE_CANCEL)

		button = rox.ButtonMixed(g.STOCK_QUIT, _("Logout"))
		button.set_flags(g.CAN_DEFAULT)
		button.show()
		self.add_action_widget(button, g.RESPONSE_YES)

		self.set_default_response(g.RESPONSE_YES)

def show_logout_box(rox_session):
	session_control = rox_session.get_object('/Session',
					'net.sf.rox.Session.Control')

	box = LogoutBox()
	resp = box.run()
	if resp == g.RESPONSE_YES:
		session_control.LogoutWithoutConfirm()
	elif resp == 1:
		session_control.ShowOptions()

	box.destroy()
