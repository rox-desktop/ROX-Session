#!/usr/bin/env python
import os
import findrox; findrox.version(1, 9, 6)
import rox
from rox import choices, filer

__builtins__._ = rox.i18n.translation(os.path.join(rox.app_dir, 'Messages'))

# Find Choices directory...
path = choices.save('ROX-Session', 'AutoStart')
if not os.path.isdir(path):
	os.mkdir(path)
	rox.info(_('Symlink any applications you want run on login into this '
		   'directory'))
filer.open_dir(path)
