#!/bin/sh

# This gets run if the user decides to make ROX-Session their default
# session manager.
#
# We set the the .xsession and .xinitrc files with sensible defaults.

cd

if [ -f .xsession ]; then
	mv .xsession xsession.old || exit 1
	echo "Made a backup of .xsession as xsession.old"
fi

if [ -f .xinitrc ]; then
	mv .xinitrc xinitrc.old || exit 1
	echo "Made a backup of .xinitrc as xinitrc.old"
fi

# They might be broken symlinks...
rm -f .xsession
rm -f .xinitrc

cat > .xsession << EOF
#!/bin/sh
# This file was created by ROX-Session.

# Step 1: Set up any environment variables you want here.

if [ -d ~/bin ]; then
	PATH=\${HOME}/bin:\${PATH}
	export PATH
fi

if [ -d ~/lib ]; then
	if [ -n "\$LD_LIBRARY_PATH" ]; then
		LD_LIBRARY_PATH=\${HOME}/lib:\${LD_LIBRARY_PATH}
	else
		LD_LIBRARY_PATH=\${HOME}/lib
	fi
	export LD_LIBRARY_PATH
fi

# Step 2: Try to run ROX-Session. If it works, stop right here.

if [ -x "$1/AppRun" ]; then
	exec "$1/AppRun" -w
fi

# Step 3: It didn't work. Try to provide a failsafe login so the user
# can fix things.

# Load a window manager. Keep trying until we find one that works!

wm=\`which sawfish\`
if [ ! -x "\$wm" ]; then wm=\`which sawmill\`; fi
if [ ! -x "\$wm" ]; then wm=\`which enlightenment\`; fi
if [ ! -x "\$wm" ]; then wm=\`which kwm\`; fi
if [ ! -x "\$wm" ]; then wm=\`which fvwm2\`; fi
if [ ! -x "\$wm" ]; then wm=\`which fvwm\`; fi
if [ ! -x "\$wm" ]; then wm=\`which 4Dwm\`; fi
if [ ! -x "\$wm" ]; then wm=\`which twm\`; fi
if [ ! -x "\$wm" ]; then wm=twm; fi
\$wm &

xmessage -file - << END
.xsession: failed to run $1/AppRun - maybe you moved or deleted it?

I'll try to give you an xterm and a filer window instead - try to find
and run ROX-Session to fix the problem. Close the xterm to logout.

If all else fails, delete your .xsession and .xinitrc files to get the
system defaults.

Report any problems to <tal197@users.sourceforge.net>.

Good luck!
END
rox &
exec xterm
EOF

chmod a+x .xsession
echo Created new .xsession file

ln -s .xsession .xinitrc || exit 1

echo Make symbolic link from .xinitrc to .xsession
