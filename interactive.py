import sys

import xxmlrpc

import rox
from rox import g
import os.path
import constants

try:
	rox_session = xxmlrpc.XXMLProxy('net.sourceforge.rox.ROX-Session')
	session_control = rox_session.get_object('/Session')
except xxmlrpc.NoSuchService:
	rox_session = None

def setup_or_logout():
	if rox_session:
		import logout
		logout.show_logout_box(session_control)
	else:
		import setup
		setup.setup_with_confirm()

def show_options():
	session_control.ShowOptions().get_response()

def show_messages():
	session_control.ShowMessages().get_response()
