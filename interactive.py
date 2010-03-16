import sys

import rox
from rox import g
import rox.session
import rox.xxmlrpc
import os.path
import constants

try:
	rox_session = rox.session.running()
	session_control = rox.session.get_session()
except rox.xxmlrpc.NoSuchService:
	rox_session = False

def setup_or_logout():
	if rox_session:
		import logout
		logout.show_logout_box(session_control)
	else:
		import setup
		setup.setup_with_confirm()

def show_options():
	session_control.ShowOptions()

def show_messages():
	session_control.ShowMessages()
