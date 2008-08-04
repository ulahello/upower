/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 David Zeuthen <david@fubar.dk>
 * Copyright (C) 2008 Richard Hughes <richard@hughsie.com>
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
#include "dkp-debug.h"
#include "dkp-object.h"

static DBusGConnection *bus = NULL;
static DBusGProxy *power_proxy = NULL;
static GMainLoop *loop;

static gboolean opt_enumerate = FALSE;
static gboolean opt_monitor = FALSE;
static gboolean opt_monitor_detail = FALSE;
static gchar *opt_show_info = FALSE;

static gboolean dkp_tool_do_monitor (void);
static void dkp_tool_show_device_info (const gchar *object_path);

/**
 * dkp_tool_device_added_cb:
 **/
static void
dkp_tool_device_added_cb (DBusGProxy *proxy, const gchar *object_path, gpointer user_data)
{
	g_print ("added:     %s\n", object_path);
	if (opt_monitor_detail) {
		dkp_tool_show_device_info (object_path);
		g_print ("\n");
	}
}

/**
 * dkp_tool_device_changed_cb:
 **/
static void
dkp_tool_device_changed_cb (DBusGProxy *proxy, const gchar *object_path, gpointer user_data)
{
	g_print ("changed:     %s\n", object_path);
	if (opt_monitor_detail) {
		/* TODO: would be nice to just show the diff */
		dkp_tool_show_device_info (object_path);
		g_print ("\n");
	}
}

/**
 * dkp_tool_device_removed_cb:
 **/
static void
dkp_tool_device_removed_cb (DBusGProxy *proxy, const gchar *object_path, gpointer user_data)
{
	g_print ("removed:   %s\n", object_path);
}

/**
 * dkp_tool_get_device_properties:
 **/
static GHashTable *
dkp_tool_get_device_properties (DBusGConnection *bus, const char *object_path)
{
	GError *error;
	GHashTable *hash_table = NULL;
	DBusGProxy *proxy;
	const char *ifname = "org.freedesktop.DeviceKit.Power.Device";

	proxy = dbus_g_proxy_new_for_name (bus, "org.freedesktop.DeviceKit.Power",
						object_path, "org.freedesktop.DBus.Properties");
	error = NULL;
	if (!dbus_g_proxy_call (proxy, "GetAll", &error,
				G_TYPE_STRING, ifname,
				G_TYPE_INVALID,
				dbus_g_type_get_map ("GHashTable", G_TYPE_STRING, G_TYPE_VALUE),
				&hash_table,
				G_TYPE_INVALID)) {
		dkp_debug ("Couldn't call GetAll() to get properties for %s: %s", object_path, error->message);
		g_error_free (error);
		goto out;
	}

out:
	g_object_unref (proxy);
	return hash_table;
}

/**
 * dkp_tool_do_monitor:
 **/
static gboolean
dkp_tool_do_monitor (void)
{
	g_print ("Monitoring activity from the power daemon. Press Ctrl+C to cancel.\n");

	dbus_g_proxy_connect_signal (power_proxy, "DeviceAdded",
				     G_CALLBACK (dkp_tool_device_added_cb), NULL, NULL);
	dbus_g_proxy_connect_signal (power_proxy, "DeviceRemoved",
				     G_CALLBACK (dkp_tool_device_removed_cb), NULL, NULL);
	dbus_g_proxy_connect_signal (power_proxy, "DeviceChanged",
				     G_CALLBACK (dkp_tool_device_changed_cb), NULL, NULL);
	g_main_loop_run (loop);

	return FALSE;
}

/**
 * dkp_tool_get_device_stats:
 **/
static gboolean
dkp_tool_get_device_stats (DBusGConnection *bus, const char *object_path, const gchar *type, guint timespec)
{
	GError *error = NULL;
	DBusGProxy *proxy;
	GType g_type_gvalue_array;
	GPtrArray *gvalue_ptr_array = NULL;
	GValueArray *gva;
	GValue *gv;
	guint i;

	proxy = dbus_g_proxy_new_for_name (bus, "org.freedesktop.DeviceKit.Power",
						object_path, "org.freedesktop.DeviceKit.Power.Source");

	g_type_gvalue_array = dbus_g_type_get_collection ("GPtrArray",
					dbus_g_type_get_struct("GValueArray",
						G_TYPE_UINT,
						G_TYPE_DOUBLE,
						G_TYPE_STRING,
						G_TYPE_INVALID));

	error = NULL;
	if (!dbus_g_proxy_call (proxy, "GetStatistics", &error,
				G_TYPE_STRING, type,
				G_TYPE_UINT, timespec,
				G_TYPE_INVALID,
				g_type_gvalue_array, &gvalue_ptr_array,
				G_TYPE_INVALID)) {
		dkp_debug ("GetStatistics(%s,%i) on %s failed: %s", type, timespec, object_path, error->message);
		g_error_free (error);
		goto out;
	}

	/* no data */
	if (gvalue_ptr_array->len == 0)
		goto out;

	guint timeval;
	gdouble value;
	const gchar *state;

	g_print ("  statistics (%s)\n", type);
	for (i=0; i< gvalue_ptr_array->len; i++) {
		gva = (GValueArray *) g_ptr_array_index (gvalue_ptr_array, i);
		/* 0 */
		gv = g_value_array_get_nth (gva, 0);
		timeval = g_value_get_uint (gv);
		g_value_unset (gv);
		/* 1 */
		gv = g_value_array_get_nth (gva, 1);
		value = g_value_get_double (gv);
		g_value_unset (gv);
		/* 2 */
		gv = g_value_array_get_nth (gva, 2);
		state = g_value_get_string (gv);
		g_print ("    %lu seconds\t%.2lf (%s)\n", time (NULL) - timeval, value, state);
		g_value_unset (gv);
		g_value_array_free (gva);
	}

out:
	if (gvalue_ptr_array != NULL)
		g_ptr_array_free (gvalue_ptr_array, TRUE);
	g_object_unref (proxy);
	return TRUE;
}

/**
 * dkp_tool_show_device_info:
 **/
static void
dkp_tool_show_device_info (const gchar *object_path)
{
	GHashTable *hash;
	DkpObject *obj;

	/* get all the properties */
	hash = dkp_tool_get_device_properties (bus, object_path);
	if (hash == NULL) {
		g_print ("Cannot get device properties for %s\n", object_path);
		return;
	}

	/* create an object and copy properties */
	obj = dkp_object_new ();
	dkp_object_set_from_map (obj, hash);

	/* print to screen */
	dkp_object_print (obj);

	/* if we can, get stats */
	dkp_tool_get_device_stats (bus, object_path, "charge", 120);
	dkp_tool_get_device_stats (bus, object_path, "rate", 120);

	/* tidy up */
	dkp_object_free (obj);
	g_hash_table_unref (hash);
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
	gboolean opt_dump = FALSE;
	unsigned int n;

	const GOptionEntry entries[] = {
		{ "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose, _("Show extra debugging information"), NULL },
		{ "enumerate", 0, 0, G_OPTION_ARG_NONE, &opt_enumerate, _("Enumerate objects paths for devices"), NULL },
		{ "dump", 0, 0, G_OPTION_ARG_NONE, &opt_dump, _("Dump all parameters for all objects"), NULL },
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

	if (opt_dump) {
		dkp_warning ("dump not supported");
	} else if (opt_enumerate) {
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
		if (!dkp_tool_do_monitor ())
			goto out;
	} else if (opt_show_info != NULL) {
		dkp_tool_show_device_info (opt_show_info);
	}

	ret = 0;

out:
	if (power_proxy != NULL)
		g_object_unref (power_proxy);
	if (bus != NULL)
		dbus_g_connection_unref (bus);

	return ret;
}
