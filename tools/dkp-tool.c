/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 David Zeuthen <davidz@redhat.com>
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
#include <glib.h>
#include <glib/gi18n-lib.h>

#include "dkp-marshal.h"
#include "egg-debug.h"
#include "dkp-client.h"
#include "dkp-client-device.h"

static GMainLoop *loop;
static gboolean opt_monitor_detail = FALSE;

/**
 * dkp_tool_device_added_cb:
 **/
static void
dkp_tool_device_added_cb (DkpClient *client, const DkpClientDevice *device, gpointer user_data)
{
	g_print ("added:     %s\n", dkp_client_device_get_object_path (device));
	if (opt_monitor_detail) {
		dkp_client_device_print (device);
	}
}

/**
 * dkp_tool_device_changed_cb:
 **/
static void
dkp_tool_device_changed_cb (DkpClient *client, const DkpClientDevice *device, gpointer user_data)
{
	g_print ("changed:     %s\n", dkp_client_device_get_object_path (device));
	if (opt_monitor_detail) {
		/* TODO: would be nice to just show the diff */
		dkp_client_device_print (device);
	}
}

/**
 * dkp_tool_device_removed_cb:
 **/
static void
dkp_tool_device_removed_cb (DkpClient *client, const DkpClientDevice *device, gpointer user_data)
{
	g_print ("removed:   %s\n", dkp_client_device_get_object_path (device));
}

/**
 * dkp_tool_do_monitor:
 **/
static gboolean
dkp_tool_do_monitor (DkpClient *client)
{
	g_print ("Monitoring activity from the power daemon. Press Ctrl+C to cancel.\n");

	g_signal_connect (client, "added", G_CALLBACK (dkp_tool_device_added_cb), NULL);
	g_signal_connect (client, "removed", G_CALLBACK (dkp_tool_device_removed_cb), NULL);
	g_signal_connect (client, "changed", G_CALLBACK (dkp_tool_device_changed_cb), NULL);

	g_main_loop_run (loop);

	return FALSE;
}

/**
 * main:
 **/
int
main (int argc, char **argv)
{
	int ret = 1;
	GOptionContext *context;
	gboolean verbose = FALSE;
	gboolean opt_dump = FALSE;
	gboolean opt_enumerate = FALSE;
	gboolean opt_monitor = FALSE;
	gchar *opt_show_info = FALSE;
	unsigned int n;

	DkpClient *client;
	DkpClientDevice *device;

	const GOptionEntry entries[] = {
		{ "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose, _("Show extra debugging information"), NULL },
		{ "enumerate", 'e', 0, G_OPTION_ARG_NONE, &opt_enumerate, _("Enumerate objects paths for devices"), NULL },
		{ "dump", 'd', 0, G_OPTION_ARG_NONE, &opt_dump, _("Dump all parameters for all objects"), NULL },
		{ "monitor", 'm', 0, G_OPTION_ARG_NONE, &opt_monitor, _("Monitor activity from the power daemon"), NULL },
		{ "monitor-detail", 0, 0, G_OPTION_ARG_NONE, &opt_monitor_detail, _("Monitor with detail"), NULL },
		{ "show-info", 'i', 0, G_OPTION_ARG_STRING, &opt_show_info, _("Show information about object path"), NULL },
		{ NULL }
	};

	g_type_init ();

	context = g_option_context_new ("DeviceKit-power tool");
	g_option_context_add_main_entries (context, entries, NULL);
	g_option_context_parse (context, &argc, &argv, NULL);
	g_option_context_free (context);
	egg_debug_init (verbose);

	loop = g_main_loop_new (NULL, FALSE);
	client = dkp_client_new ();

	if (opt_enumerate || opt_dump) {
		GPtrArray *devices;
		const gchar *object_path;
		devices = dkp_client_enumerate_devices (client, NULL);
		if (devices == NULL)
			goto out;
		for (n=0; n < devices->len; n++) {
			object_path = (const gchar *) g_ptr_array_index (devices, n);
			if (opt_enumerate)
				g_print ("%s\n", object_path);
			else {
				g_print ("Device: %s\n", object_path);
				device = dkp_client_device_new ();
				dkp_client_device_set_object_path (device, object_path);
				dkp_client_device_print (device);
				g_object_unref (device);
			}
		}
		g_ptr_array_foreach (devices, (GFunc) g_free, NULL);
		g_ptr_array_free (devices, TRUE);
	} else if (opt_monitor || opt_monitor_detail) {
		if (!dkp_tool_do_monitor (client))
			goto out;
	} else if (opt_show_info != NULL) {
		device = dkp_client_device_new ();
		dkp_client_device_set_object_path (device, opt_show_info);
		dkp_client_device_print (device);
		g_object_unref (device);
	}

	ret = 0;
out:
	g_object_unref (client);
	return ret;
}
