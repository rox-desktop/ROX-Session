/*
 * $Id$
 *
 * ROX-Session, a very simple session manager
 * Copyright (C) 2002, Thomas Leonard, <tal197@users.sourceforge.net>.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* See http://www.freedesktop.org for details. */

#include "config.h"

#define DBUS_API_SUBJECT_TO_CHANGE
#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>

#include <gdk/gdkx.h>

#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <gtk/gtk.h>

#include "global.h"

#include "dbus.h"
#include "dpms.h"
#include "settings.h"
#include "session.h"
#include "choices.h"
#include "gui_support.h"
#include "wm.h"
#include "xkb.h"

#define ROX_XSETTINGS_NS "net.sf.rox.Session.Settings"

#define SETTINGS_FILE "Settings.xml"

#define MISSING_SETTING_ERROR "net.sf.rox.Session.MissingSetting"

XSettingsManager *xsettings_manager = NULL;

static xmlDoc *settings_doc = NULL;

static int mouse_accel_factor = 20, mouse_accel_threshold = 10;
static gboolean kbd_repeat = TRUE;
static int kbd_delay = 500, kbd_interval = 30;
static int dpms_standby_time = 15 * 60;
static int dpms_suspend_time = 20 * 60;
static int dpms_off_time = 30 * 60;


/* Static prototypes */
static void terminate_xsettings(void *data);
static void set_from_xml(xmlNode *setting);
static DBusMessage *xsettings_handler(DBusMessage *message, DBusError *error);
static void activate_changes(void);
static xmlNode *get_node(const char *name, gboolean create_if_missing);
static void save_settings(void);

/****************************************************************
 *			EXTERNAL INTERFACE			*
 ****************************************************************/

void settings_init(void)
{
	const char *xsettings_path[] = {"Settings", NULL};
	char *path;

	register_object_path(xsettings_path, xsettings_handler);

	if (xsettings_manager_check_running(gdk_display,
					    DefaultScreen(gdk_display)))
	{
		g_printerr("An XSETTINGS manager is already running. "
				"Not taking control of XSETTINGS...\n");
		return;
	}
	else
		xsettings_manager = xsettings_manager_new(gdk_display,
						DefaultScreen(gdk_display),
					        terminate_xsettings, NULL);

	path = choices_find_path_load(SETTINGS_FILE, PROJECT);
	if (path)
	{
		settings_doc = xmlParseFile(path);
		g_free(path);
	}

	if (settings_doc)
	{
		xmlNode *root = xmlDocGetRootElement(settings_doc);
		xmlNode *child;

		for (child = root->xmlChildrenNode; child; child = child->next)
		{
			if (child->type != XML_ELEMENT_NODE)
				continue;
			if (strcmp(child->name, "Setting") != 0)
				continue;

			set_from_xml(child);
		}
	}
	else
	{
		settings_doc = xmlNewDoc("1.0");
		xmlDocSetRootElement(settings_doc,
				xmlNewDocNode(settings_doc, NULL, "Settings", NULL));
		/* Override annoying defaults... */
		xsettings_manager_set_int(xsettings_manager, "Gtk/CanChangeAccels", 1);
		xsettings_manager_set_string(xsettings_manager,
						"Gtk/KeyThemeName", "Emacs");
	}

	activate_changes();
}

void settings_set_string(const char *name, char *value)
{
	xmlNode *node;

	node = get_node(name, TRUE);
	xmlSetProp(node, "value", value);
	xmlSetProp(node, "type", "string");

	set_from_xml(node);
	activate_changes();

	save_settings();
}

/****************************************************************
 *			INTERNAL FUNCTIONS			*
 ****************************************************************/

static void set_rox_setting(const char *name, const char *value)
{
	if (strcmp(name, "AccelThreshold") == 0)
		mouse_accel_threshold = atoi(value);
	else if (strcmp(name, "AccelFactor") == 0)
		mouse_accel_factor = atoi(value);
	else if (strcmp(name, "WindowManager") == 0)
		set_window_manager(value);
	else if (strcmp(name, "KeyTable") == 0)
		set_xkb_layout(value);
	else if (strcmp(name, "KbdRepeat") == 0)
		kbd_repeat = atoi(value);
	else if (strcmp(name, "KbdDelay") == 0)
		kbd_delay = atoi(value);
	else if (strcmp(name, "KbdInterval") == 0)
		kbd_interval = atoi(value);
	else if (strcmp(name, "DPMSEnable") == 0)
		dpms_set_enabled(GDK_DISPLAY(), atoi(value));
	else if (strcmp(name, "DPMSStandby") == 0)
		dpms_standby_time = 60 * atoi(value);
	else if (strcmp(name, "DPMSSuspend") == 0)
		dpms_suspend_time = 60 * atoi(value);
	else if (strcmp(name, "DPMSOff") == 0)
		dpms_off_time = 60 * atoi(value);
	else
		g_warning("Unknown ROX setting 'ROX/%s'", name);
}

static void activate_changes(void)
{
	g_return_if_fail(xsettings_manager != NULL);

	xsettings_manager_notify(xsettings_manager);
	XChangePointerControl(GDK_DISPLAY(), True, True,
				mouse_accel_factor, 10,
				mouse_accel_threshold);
	set_xkb_repeat(kbd_repeat, kbd_delay, kbd_interval);
	dpms_set_times(GDK_DISPLAY(),
			dpms_standby_time, dpms_suspend_time, dpms_off_time);
}

/* Find the setting element in settings_doc, or create a new one. */
static xmlNode *get_node(const char *name, gboolean create_if_missing)
{
	xmlNode *root = xmlDocGetRootElement(settings_doc);
	xmlNode *child;

	for (child = root->xmlChildrenNode; child; child = child->next)
	{
		char *attr_name;
		gboolean same;

		if (child->type != XML_ELEMENT_NODE)
			continue;
		if (strcmp(child->name, "Setting") != 0)
			continue;
		attr_name = xmlGetProp(child, "name");
		if (!attr_name)
		{
			g_warning("Malformed <Setting>");
			continue;
		}
		same = strcmp(attr_name, name) == 0;
		g_free(attr_name);

		if (same)
			return child;
	}

	if (!create_if_missing)
		return NULL;

	child = xmlNewDocNode(settings_doc, NULL, "Setting", NULL);
	xmlSetProp(child, "name", name);
	xmlAddChild(root, child);
	return child;
}

static void save_settings(void)
{
	char *save, *save_new;
	
	save = choices_find_path_save(SETTINGS_FILE, PROJECT, TRUE);
	if (!save)
		return;

	save_new = g_strconcat(save, ".new", NULL);
	if (save_xml_file(settings_doc, save_new) || rename(save_new, save))
		report_error(_("Error saving %s: %s"), save, g_strerror(errno));

	g_free(save_new);
	g_free(save);
}

static void set_int(const char *name, int value)
{
	xmlNode *node;
	char *str;

	str = g_strdup_printf("%d", value);

	node = get_node(name, TRUE);
	xmlSetProp(node, "value", str);
	xmlSetProp(node, "type", "int");

	g_free(str);
	
	set_from_xml(node);
	activate_changes();

	save_settings();
}

static DBusMessage *xsettings_handler(DBusMessage *message, DBusError *error)
{
	if (xsettings_manager == NULL)
	{
		return dbus_message_new_error(message, ROX_SESSION_ERROR,
				"ROX-Session is not currently managing the XSettings");
	}
	
	if (dbus_message_is_method_call(message, ROX_XSETTINGS_NS, "SetInt"))
	{
		char *name = NULL;
		gint32 value = 0;

		if (!dbus_message_get_args(message, error,
					DBUS_TYPE_STRING, &name,
					DBUS_TYPE_INT32, &value,
					DBUS_TYPE_INVALID))
			return NULL;

		set_int(name, value);

		return dbus_message_new_method_return(message);
	}
	else if (dbus_message_is_method_call(message, ROX_XSETTINGS_NS, "SetString"))
	{
		char *name = NULL;
		char *value = NULL;

		if (!dbus_message_get_args(message, error,
					DBUS_TYPE_STRING, &name,
					DBUS_TYPE_STRING, &value,
					DBUS_TYPE_INVALID))
			return NULL;

		settings_set_string(name, value);

		return dbus_message_new_method_return(message);
	}
	else if (dbus_message_is_method_call(message, ROX_XSETTINGS_NS, "GetSetting"))
	{
		DBusMessage *reply;
		char *name = NULL;
		char *type, *value;
		xmlNode *node;

		if (!dbus_message_get_args(message, error,
					DBUS_TYPE_STRING, &name,
					DBUS_TYPE_INVALID))
			return NULL;

		node = get_node(name, FALSE);
		g_free(name);

		if (!node)
			return dbus_message_new_error(message, MISSING_SETTING_ERROR,
					"Missing setting");

		type = xmlGetProp(node, "type");
		value = xmlGetProp(node, "value");
		if (type && value)
		{
			reply = dbus_message_new_method_return(message);
			if (!dbus_message_append_args(reply,
							DBUS_TYPE_STRING, type,
							DBUS_TYPE_STRING, value,
							DBUS_TYPE_INVALID)) {
				dbus_message_unref(reply);
				reply = dbus_message_new_error(message, ROX_SESSION_ERROR,
						"Out of memory");
			}
		}
		else
			reply = dbus_message_new_error(message, ROX_SESSION_ERROR,
					"Setting is malformed");
		g_free(type);
		g_free(value);

		return reply;
	}

	return NULL;
}

static void terminate_xsettings(void *data)
{
	g_warning("ROX-Session is no longer the XSETTINGS manager!");
}

static void set_from_xml(xmlNode *setting)
{
	char *name, *value, *type;

	g_return_if_fail(xsettings_manager != NULL);

	name = xmlGetProp(setting, "name");
	type = xmlGetProp(setting, "type");
	value = xmlGetProp(setting, "value");

	g_return_if_fail(name && type && value);

	/* The ROX/ settings aren't actually XSettings, but are still set in
	 * the same way.
	 */
	if (strncmp(name, "ROX/", 4) == 0)
	{
		set_rox_setting(name + 4, value);
		return;
	}

	if (strcmp(type, "string") == 0)
		xsettings_manager_set_string(xsettings_manager, name, value);
	else if (strcmp(type, "int") == 0)
		xsettings_manager_set_int(xsettings_manager, name, atoi(value));
	else
		g_warning("Unknown type '%s' in Settings file", type);
}
