/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 David Zeuthen <david@fubar.dk>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <string.h>

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <glib-object.h>

#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <devkit-gobject.h>

#include "dkp-daemon.h"

#define NAME_TO_CLAIM "org.freedesktop.DeviceKit.Power"

/**
 * main_acquire_name_on_proxy:
 **/
static gboolean
main_acquire_name_on_proxy (DBusGProxy *bus_proxy)
{
	GError *error;
	guint	  result;
	gboolean	res;
	gboolean	ret;

	ret = FALSE;

	if (bus_proxy == NULL) {
		goto out;
	}

	error = NULL;
	res = dbus_g_proxy_call (bus_proxy,
				 "RequestName",
				 &error,
				 G_TYPE_STRING, NAME_TO_CLAIM,
				 G_TYPE_UINT, 0,
				 G_TYPE_INVALID,
				 G_TYPE_UINT, &result,
				 G_TYPE_INVALID);
	if (!res) {
		if (error != NULL) {
			g_warning ("Failed to acquire %s: %s", NAME_TO_CLAIM, error->message);
			g_error_free (error);
		} else {
			g_warning ("Failed to acquire %s", NAME_TO_CLAIM);
		}
		goto out;
	}

 	if (result != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
		if (error != NULL) {
			g_warning ("Failed to acquire %s: %s", NAME_TO_CLAIM, error->message);
			g_error_free (error);
		} else {
			g_warning ("Failed to acquire %s", NAME_TO_CLAIM);
		}
		goto out;
	}

	ret = TRUE;

 out:
	return ret;
}

/**
 * main:
 **/
int
main (int argc, char **argv)
{
	GError	 *error;
	GMainLoop	 *loop;
	DkpDaemon *power_daemon;
	GOptionContext *context;
	DBusGProxy	 *bus_proxy;
	DBusGConnection *bus;
	int		  ret;
	static GOptionEntry  entries []   = {
		{ NULL }
	};

	ret = 1;

	g_type_init ();

	context = g_option_context_new ("DeviceKit Power Daemon");
	g_option_context_add_main_entries (context, entries, NULL);
	g_option_context_parse (context, &argc, &argv, NULL);
	g_option_context_free (context);

	error = NULL;
	bus = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
	if (bus == NULL) {
		g_warning ("Couldn't connect to system bus: %s", error->message);
		g_error_free (error);
		goto out;
	}

	bus_proxy = dbus_g_proxy_new_for_name (bus,
						  DBUS_SERVICE_DBUS,
						  DBUS_PATH_DBUS,
						  DBUS_INTERFACE_DBUS);
	if (bus_proxy == NULL) {
		g_warning ("Could not construct bus_proxy object; bailing out");
		goto out;
	}

	if (!main_acquire_name_on_proxy (bus_proxy) ) {
		g_warning ("Could not acquire name; bailing out");
		goto out;
	}

	g_debug ("Starting devkit-power-daemon version %s", VERSION);

	power_daemon = dkp_daemon_new ();

	if (power_daemon == NULL) {
		goto out;
	}

	loop = g_main_loop_new (NULL, FALSE);

	g_main_loop_run (loop);

	g_object_unref (power_daemon);
	g_main_loop_unref (loop);
	ret = 0;

out:
	return ret;
}
