import rox
from rox import g
from rox.options import Option
import os, sys

import session

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

def op_button(text, stock, command, message, dialog = None):
	button = g.Button()

	hbox = g.HBox(False, 0)
	button.add(hbox)

	image = g.Image()
	image.set_from_stock(stock, g.ICON_SIZE_BUTTON)
	hbox.pack_start(image, False, True, 4)

	label = g.Label('')
	label.set_text_with_mnemonic(text)
	label.set_alignment(0, 0.5)
	hbox.pack_start(label, True, True, 0)

	# No need for clickable buttons if no command is set
	tmp = command.strip()
	if tmp:
		def invoke(button):
			print >>sys.stderr, message
			os.system(command)
			if dialog:
				dialog.response(g.RESPONSE_ACCEPT)
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

		button = op_button(_('_Halt'), 'rox-halt',
			session.o_halt.value,
			_('Attempting to halt the system...'))
		hbox.pack_end(button, False, True, 0)

		button = op_button(_('_Reboot'), g.STOCK_REFRESH,
			session.o_reboot.value,
			_('Attempting to restart the system...'))
		hbox.pack_end(button, False, True, 0)
		
		button = op_button(_('_Sleep'), 'rox-suspend',
			session.o_suspend.value,
			_('Attempting to enter suspend mode...'), self)
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

def show_logout_box(session_control):
	box = LogoutBox()
	resp = box.run()
	if resp == int(g.RESPONSE_YES):
		session_control.LogoutWithoutConfirm()
	elif resp == 1:
		session_control.ShowOptions()

	box.destroy()
