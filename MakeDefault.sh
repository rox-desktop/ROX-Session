#!/bin/sh

# This gets run if the user decides to make ROX-Session their default
# session manager.
#
# We set the the .xsession and .xinitrc files with sensible defaults.

cd

if [ -e .xsession ]; then
	mv .xsession xsession.old || exit 1
	echo "Made a backup of .xsession in xsession.old"
fi

if [ -L .xinitrc ]; then
	rm .xinitrc || exit 1
	echo "Removed symbolic link .xinitrc"
fi

if [ -e .xinitrc ]; then
	mv .xinitrc xinitrc.old || exit 1
	echo "Made a backup of .xinitrc in xinitrc.old"
fi

cat > .xsession << EOF
#!/bin/sh
# This file was created by ROX-Session.

# Step 1: Set up any environment variables you want here.
# Eg 'PATH=\${HOME}/bin:\${PATH}'.

# Step 2: Load a window manager. Keep trying until we find one that works!
# You can choose yourself by changing the first line to your preferred
# window manager.

wm=\`which sawfish\`
if [ -z "\$wm" ]; then wm=\`which sawmill\`; fi
if [ -z "\$wm" ]; then wm=\`which enlightenment\`; fi
if [ -z "\$wm" ]; then wm=\`which fvwm2\`; fi
if [ -z "\$wm" ]; then wm=\`which fvwm\`; fi
if [ -z "\$wm" ]; then wm=\`which 4Dwm\`; fi
if [ -z "\$wm" ]; then wm=\`which twm\`; fi
\$wm &

# Step 3: Load any other programs you want at start-up time.
# Put an & at the end of each command. Eg 'xclock &'.

# If the panel and pinboard icons aren't handled correctly, try changing
# -b to -ob.
rox -b MyPanel -p MyPinboard &

# Step 4: Load ROX-Session. When it quits the session is over.
# If that doesn't work, try to find something that will at least
# allow the user to fix their session!

no_exit_on_failed_exec=1
exec "$1/AppRun" -w

twm &

xmessage "ROX-Session ($1/AppRun) didn't run!"

exec xterm
exec gnome-terminal
exec konsole
EOF

chmod a+x .xsession
echo Created new .xsession file

ln -s .xsession .xinitrc || exit 1

echo Make symbolic link from .xinitrc to .xsession
