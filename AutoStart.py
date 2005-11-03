import rox
from rox import basedir, filer
import constants

# Find Choices directory...
path = basedir.save_config_path(constants.site, 'ROX-Session', 'AutoStart')
rox.info(_('Symlink any applications you want run on login into this '
	   'directory'))
filer.open_dir(path)
