CC = gcc
CFLAGS = -Wall -g `gtk-config --cflags`
LDFLAGS = -L/usr/X11/lib -lX11 `gtk-config --libs`

PLATFORM_DIR = ../$(PLATFORM)
EXEC = $(PLATFORM_DIR)/ROX-Session
SRCS = main.c
OBJECTS = main.o

unknown:
	PLATFORM=`(unset HOSTTYPE; unset OSTYPE; bash -c 'echo $$OSTYPE-$$HOSTTYPE')` ; export PLATFORM; make known

known: $(EXEC)

depend:
	makedepend -- $(CFLAGS) -- $(SRCS)

$(PLATFORM_DIR):
	mkdir $(PLATFORM_DIR)

$(EXEC): $(PLATFORM_DIR) $(OBJECTS)
	$(CC) -o $(EXEC) $(OBJECTS) $(LDFLAGS)
# DO NOT DELETE

main.o: /usr/include/gtk/gtk.h /usr/include/gdk/gdk.h
main.o: /usr/include/gdk/gdktypes.h /usr/include/glib.h
main.o: /usr/lib/glib/include/glibconfig.h /usr/include/limits.h
main.o: /usr/include/features.h /usr/include/sys/cdefs.h
main.o: /usr/include/gnu/stubs.h /usr/include/bits/posix1_lim.h
main.o: /usr/include/bits/local_lim.h /usr/include/linux/limits.h
main.o: /usr/include/bits/posix2_lim.h
main.o: /usr/lib/gcc-lib/i386-redhat-linux/egcs-2.91.66/include/float.h
main.o: /usr/lib/gcc-lib/i386-redhat-linux/egcs-2.91.66/include/stdarg.h
main.o: /usr/include/gdk/gdkcursors.h /usr/include/gdk/gdkrgb.h
main.o: /usr/include/gtk/gtkaccelgroup.h /usr/include/gtk/gtkobject.h
main.o: /usr/include/gtk/gtkarg.h /usr/include/gtk/gtktypeutils.h
main.o: /usr/include/gtk/gtktypebuiltins.h /usr/include/gtk/gtkenums.h
main.o: /usr/include/gtk/gtkdebug.h /usr/include/gtk/gtkaccellabel.h
main.o: /usr/include/gtk/gtklabel.h /usr/include/gtk/gtkmisc.h
main.o: /usr/include/gtk/gtkwidget.h /usr/include/gtk/gtkadjustment.h
main.o: /usr/include/gtk/gtkdata.h /usr/include/gtk/gtkstyle.h
main.o: /usr/include/gtk/gtkalignment.h /usr/include/gtk/gtkbin.h
main.o: /usr/include/gtk/gtkcontainer.h /usr/include/gtk/gtkaspectframe.h
main.o: /usr/include/gtk/gtkframe.h /usr/include/gtk/gtkarrow.h
main.o: /usr/include/gtk/gtkbindings.h /usr/include/gtk/gtkbox.h
main.o: /usr/include/gtk/gtkbbox.h /usr/include/gtk/gtkbutton.h
main.o: /usr/include/gtk/gtkcalendar.h /usr/include/gtk/gtksignal.h
main.o: /usr/include/gtk/gtkmarshal.h /usr/include/gtk/gtkcheckbutton.h
main.o: /usr/include/gtk/gtktogglebutton.h
main.o: /usr/include/gtk/gtkcheckmenuitem.h /usr/include/gtk/gtkmenuitem.h
main.o: /usr/include/gtk/gtkitem.h /usr/include/gtk/gtkclist.h
main.o: /usr/include/gtk/gtkhscrollbar.h /usr/include/gtk/gtkscrollbar.h
main.o: /usr/include/gtk/gtkrange.h /usr/include/gtk/gtkvscrollbar.h
main.o: /usr/include/gtk/gtkcolorsel.h /usr/include/gtk/gtkwindow.h
main.o: /usr/include/gtk/gtkvbox.h /usr/include/gtk/gtkpreview.h
main.o: /usr/include/gtk/gtkentry.h /usr/include/gtk/gtkeditable.h
main.o: /usr/include/gtk/gtkhbox.h /usr/include/gtk/gtkmain.h
main.o: /usr/include/gtk/gtkscale.h /usr/include/gtk/gtkhscale.h
main.o: /usr/include/gtk/gtktable.h /usr/include/gtk/gtkeventbox.h
main.o: /usr/include/gtk/gtkcombo.h /usr/include/gtk/gtkcompat.h
main.o: /usr/include/gtk/gtkctree.h /usr/include/gtk/gtkcurve.h
main.o: /usr/include/gtk/gtkdrawingarea.h /usr/include/gtk/gtkdialog.h
main.o: /usr/include/gtk/gtkdnd.h /usr/include/gtk/gtkselection.h
main.o: /usr/include/gtk/gtkfeatures.h /usr/include/gtk/gtkfilesel.h
main.o: /usr/include/gtk/gtkfixed.h /usr/include/gtk/gtkfontsel.h
main.o: /usr/include/gtk/gtknotebook.h /usr/include/gtk/gtkgamma.h
main.o: /usr/include/gtk/gtkgc.h /usr/include/gtk/gtkhandlebox.h
main.o: /usr/include/gtk/gtkhbbox.h /usr/include/gtk/gtkhpaned.h
main.o: /usr/include/gtk/gtkpaned.h /usr/include/gtk/gtkhruler.h
main.o: /usr/include/gtk/gtkruler.h /usr/include/gtk/gtkhseparator.h
main.o: /usr/include/gtk/gtkseparator.h /usr/include/gtk/gtkimage.h
main.o: /usr/include/gtk/gtkinputdialog.h /usr/include/gtk/gtkitemfactory.h
main.o: /usr/include/gtk/gtkmenufactory.h /usr/include/gtk/gtklayout.h
main.o: /usr/include/gtk/gtklist.h /usr/include/gtk/gtklistitem.h
main.o: /usr/include/gtk/gtkmenu.h /usr/include/gtk/gtkmenushell.h
main.o: /usr/include/gtk/gtkmenubar.h /usr/include/gtk/gtkoptionmenu.h
main.o: /usr/include/gtk/gtkpacker.h /usr/include/gtk/gtkpixmap.h
main.o: /usr/include/gtk/gtkplug.h /usr/include/gtk/gtkprogress.h
main.o: /usr/include/gtk/gtkprogressbar.h /usr/include/gtk/gtkradiobutton.h
main.o: /usr/include/gtk/gtkradiomenuitem.h /usr/include/gtk/gtkrc.h
main.o: /usr/include/gtk/gtkscrolledwindow.h /usr/include/gtk/gtkviewport.h
main.o: /usr/include/gtk/gtksocket.h /usr/include/gtk/gtkspinbutton.h
main.o: /usr/include/gtk/gtkstatusbar.h /usr/include/gtk/gtktearoffmenuitem.h
main.o: /usr/include/gtk/gtktext.h /usr/include/gtk/gtkthemes.h
main.o: /usr/include/gtk/gtktipsquery.h /usr/include/gtk/gtktoolbar.h
main.o: /usr/include/gtk/gtktooltips.h /usr/include/gtk/gtktree.h
main.o: /usr/include/gtk/gtktreeitem.h /usr/include/gtk/gtkvbbox.h
main.o: /usr/include/gtk/gtkvpaned.h /usr/include/gtk/gtkvruler.h
main.o: /usr/include/gtk/gtkvscale.h /usr/include/gtk/gtkvseparator.h
