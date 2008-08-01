/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 David Zeuthen <david@fubar.dk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pwd.h>
#include <grp.h>

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <glib-object.h>

#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include <polkit-dbus/polkit-dbus.h>

#include "dkp-daemon-glue.h"
#include "dkp-marshal.h"
#include "../src/dkp-debug.h"

static DBusGConnection *bus = NULL;
static DBusGProxy *power_proxy = NULL;
static GMainLoop *loop;

static gboolean opt_enumerate = FALSE;
static gboolean opt_monitor = FALSE;
static gboolean opt_monitor_detail = FALSE;
static gchar *opt_show_info = FALSE;

static gboolean do_monitor (void);
static void do_show_info (const gchar *object_path);

/**
 * device_added_signal_handler:
 **/
static void
device_added_signal_handler (DBusGProxy *proxy, const gchar *object_path, gpointer user_data)
{
	g_print ("added:     %s\n", object_path);
	if (opt_monitor_detail) {
		do_show_info (object_path);
		g_print ("\n");
	}
}

/**
 * device_changed_signal_handler:
 **/
static void
device_changed_signal_handler (DBusGProxy *proxy, const gchar *object_path, gpointer user_data)
{
	g_print ("changed:     %s\n", object_path);
	if (opt_monitor_detail) {
		/* TODO: would be nice to just show the diff */
		do_show_info (object_path);
		g_print ("\n");
	}
}

/**
 * device_removed_signal_handler:
 **/
static void
device_removed_signal_handler (DBusGProxy *proxy, const gchar *object_path, gpointer user_data)
{
	g_print ("removed:   %s\n", object_path);
}

/* --- SUCKY CODE BEGIN --- */

/* This totally sucks; dbus-bindings-tool and dbus-glib should be able
 * to do this for us.
 */

typedef struct
{
	gchar		*native_path;
	gchar		*vendor;
	gchar		*model;
	gchar		*serial;
	guint64		 update_time;
	gchar		*type;
	gboolean	 line_power_online;
	gdouble		 battery_energy;
	gdouble		 battery_energy_empty;
	gdouble		 battery_energy_empty_design;
	gdouble		 battery_energy_full;
	gdouble		 battery_energy_full_design;
	gdouble		 battery_energy_rate;
	gint64		 battery_time_to_full;
	gint64		 battery_time_to_empty;
	gdouble		 battery_percentage;
	gchar		*battery_technology;
} DeviceProperties;

/**
 * collect_props:
 **/
static void
collect_props (const char *key, const GValue *value, DeviceProperties *props)
{
	gboolean handled = TRUE;

	if (strcmp (key, "native-path") == 0)
		props->native_path = g_strdup (g_value_get_string (value));
	else if (strcmp (key, "vendor") == 0)
		props->vendor = g_strdup (g_value_get_string (value));
	else if (strcmp (key, "model") == 0)
		props->model = g_strdup (g_value_get_string (value));
	else if (strcmp (key, "serial") == 0)
		props->serial = g_strdup (g_value_get_string (value));
	else if (strcmp (key, "update-time") == 0)
		props->update_time = g_value_get_uint64 (value);
	else if (strcmp (key, "type") == 0)
		props->type = g_strdup (g_value_get_string (value));
	else if (strcmp (key, "line-power-online") == 0)
		props->line_power_online = g_value_get_boolean (value);
	else if (strcmp (key, "battery-energy") == 0)
		props->battery_energy = g_value_get_double (value);
	else if (strcmp (key, "battery-energy-empty") == 0)
		props->battery_energy_empty = g_value_get_double (value);
	else if (strcmp (key, "battery-energy-empty-design") == 0)
		props->battery_energy_empty_design = g_value_get_double (value);
	else if (strcmp (key, "battery-energy-full") == 0)
		props->battery_energy_full = g_value_get_double (value);
	else if (strcmp (key, "battery-energy-full-design") == 0)
		props->battery_energy_full_design = g_value_get_double (value);
	else if (strcmp (key, "battery-energy-rate") == 0)
		props->battery_energy_rate = g_value_get_double (value);
	else if (strcmp (key, "battery-time-to-full") == 0)
		props->battery_time_to_full = g_value_get_int64 (value);
	else if (strcmp (key, "battery-time-to-empty") == 0)
		props->battery_time_to_empty = g_value_get_int64 (value);
	else if (strcmp (key, "battery-percentage") == 0)
		props->battery_percentage = g_value_get_double (value);
	else if (strcmp (key, "battery-technology") == 0)
		props->battery_technology = g_strdup (g_value_get_string (value));
	else
		handled = FALSE;

	if (!handled)
		dkp_warning ("unhandled property '%s'", key);
}

/**
 * device_properties_get:
 **/
static DeviceProperties *
device_properties_get (DBusGConnection *bus, const char *object_path)
{
	DeviceProperties *props;
	GError *error;
	GHashTable *hash_table;
	DBusGProxy *prop_proxy;
	const char *ifname = "org.freedesktop.DeviceKit.Power.Device";

	props = g_new0 (DeviceProperties, 1);

	prop_proxy = dbus_g_proxy_new_for_name (bus, "org.freedesktop.DeviceKit.Power",
						object_path, "org.freedesktop.DBus.Properties");
	error = NULL;
	if (!dbus_g_proxy_call (prop_proxy, "GetAll", &error,
				G_TYPE_STRING, ifname,
				G_TYPE_INVALID,
				dbus_g_type_get_map ("GHashTable", G_TYPE_STRING, G_TYPE_VALUE),
				&hash_table,
				G_TYPE_INVALID)) {
		dkp_warning ("Couldn't call GetAll() to get properties for %s: %s", object_path, error->message);
		g_error_free (error);
		goto out;
	}

	g_hash_table_foreach (hash_table, (GHFunc) collect_props, props);
	g_hash_table_unref (hash_table);

out:
	g_object_unref (prop_proxy);
	return props;
}

/**
 * device_properties_free:
 **/
static void
device_properties_free (DeviceProperties *props)
{
	g_free (props->native_path);
	g_free (props->vendor);
	g_free (props->model);
	g_free (props->serial);
	g_free (props->type);
	g_free (props->battery_technology);
	g_free (props);
}

/* --- SUCKY CODE END --- */

/**
 * do_monitor:
 **/
static gboolean
do_monitor (void)
{
	g_print ("Monitoring activity from the power daemon. Press Ctrl+C to cancel.\n");

	dbus_g_proxy_connect_signal (power_proxy, "DeviceAdded",
				     G_CALLBACK (device_added_signal_handler), NULL, NULL);
	dbus_g_proxy_connect_signal (power_proxy, "DeviceRemoved",
				     G_CALLBACK (device_removed_signal_handler), NULL, NULL);
	dbus_g_proxy_connect_signal (power_proxy, "DeviceChanged",
				     G_CALLBACK (device_changed_signal_handler), NULL, NULL);
	g_main_loop_run (loop);

	return FALSE;
}

/**
 * do_show_info:
 **/
static void
do_show_info (const gchar *object_path)
{
	DeviceProperties *props;
	struct tm *time_tm;
	time_t t;
	gchar time_buf[256];

	props = device_properties_get (bus, object_path);

	t = (time_t) props->update_time;
	time_tm = localtime (&t);
	strftime (time_buf, sizeof time_buf, "%c", time_tm);

	g_print ("Showing information for %s\n", object_path);
	g_print ("  native-path:          %s\n", props->native_path);
	g_print ("  vendor:               %s\n", props->vendor);
	g_print ("  model:                %s\n", props->model);
	g_print ("  serial:               %s\n", props->serial);
	g_print ("  updated:              %s (%d seconds ago)\n", time_buf, (int) (time (NULL) - props->update_time));
	if (strcmp (props->type, "battery") == 0) {
		g_print ("  battery\n");
		g_print ("    energy:              %g Wh\n", props->battery_energy);
		g_print ("    energy-empty:        %g Wh\n", props->battery_energy_empty);
		g_print ("    energy-empty-design: %g Wh\n", props->battery_energy_empty_design);
		g_print ("    energy-full:         %g Wh\n", props->battery_energy_full);
		g_print ("    energy-full-design:  %g Wh\n", props->battery_energy_full_design);
		g_print ("    energy-rate:         %g W\n", props->battery_energy_rate);
		g_print ("    time to full:        ");
		if (props->battery_time_to_full >= 0)
			g_print ("%d seconds\n", (int) props->battery_time_to_full);
		else
			g_print ("unknown\n");
		g_print ("    time to empty:       ");
		if (props->battery_time_to_empty >= 0)
			g_print ("%d seconds\n", (int) props->battery_time_to_empty);
		else
			g_print ("unknown\n");
		g_print ("    percentage:          %g%%\n", props->battery_percentage);
		g_print ("    technology:          %s\n", props->battery_technology);
	} else if (strcmp (props->type, "line-power") == 0) {
		g_print ("  line-power\n");
		g_print ("    online:             %s\n", props->line_power_online ? "yes" : "no");
	} else {
		g_print ("  unknown power source type '%s'\n", props->type);
	}
	device_properties_free (props);
}

/**
 * main:
 **/
int
main (int argc, char **argv)
{
	int ret = 1;
	GOptionContext *context;
	GError *error = NULL;
	gboolean verbose = FALSE;
	unsigned int n;

	const GOptionEntry entries[] = {
		{ "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose, _("Show extra debugging information"), NULL },
		{ "enumerate", 0, 0, G_OPTION_ARG_NONE, &opt_enumerate, _("Enumerate objects paths for devices"), NULL },
		{ "monitor", 0, 0, G_OPTION_ARG_NONE, &opt_monitor, _("Monitor activity from the power daemon"), NULL },
		{ "monitor-detail", 0, 0, G_OPTION_ARG_NONE, &opt_monitor_detail, _("Monitor with detail"), NULL },
		{ "show-info", 0, 0, G_OPTION_ARG_STRING, &opt_show_info, _("Show information about object path"), NULL },
		{ NULL }
	};

	g_type_init ();

	context = g_option_context_new ("DeviceKit-power tool");
	g_option_context_add_main_entries (context, entries, NULL);
	g_option_context_parse (context, &argc, &argv, NULL);
	g_option_context_free (context);
	dkp_debug_init (verbose);

	loop = g_main_loop_new (NULL, FALSE);

	bus = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
	if (bus == NULL) {
		dkp_warning ("Couldn't connect to system bus: %s", error->message);
		g_error_free (error);
		goto out;
	}

	power_proxy = dbus_g_proxy_new_for_name (bus, "org.freedesktop.DeviceKit.Power",
						 "/", "org.freedesktop.DeviceKit.Power");
	dbus_g_proxy_add_signal (power_proxy, "DeviceAdded", G_TYPE_STRING, G_TYPE_INVALID);
	dbus_g_proxy_add_signal (power_proxy, "DeviceRemoved", G_TYPE_STRING, G_TYPE_INVALID);
	dbus_g_proxy_add_signal (power_proxy, "DeviceChanged", G_TYPE_STRING, G_TYPE_INVALID);

	if (opt_enumerate) {
		GPtrArray *devices;
		if (!org_freedesktop_DeviceKit_Power_enumerate_devices (power_proxy, &devices, &error)) {
			dkp_warning ("Couldn't enumerate devices: %s", error->message);
			g_error_free (error);
			goto out;
		}
		for (n = 0; n < devices->len; n++) {
			gchar *object_path = devices->pdata[n];
			g_print ("%s\n", object_path);
		}
		g_ptr_array_foreach (devices, (GFunc) g_free, NULL);
		g_ptr_array_free (devices, TRUE);
	} else if (opt_monitor || opt_monitor_detail) {
		if (!do_monitor ())
			goto out;
	} else if (opt_show_info != NULL) {
		do_show_info (opt_show_info);
	}

	ret = 0;

out:
	if (power_proxy != NULL)
		g_object_unref (power_proxy);
	if (bus != NULL)
		dbus_g_connection_unref (bus);

	return ret;
}
