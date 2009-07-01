/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 David Zeuthen <davidz@redhat.com>
 * Copyright (C) 2008 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU General Public License Version 2
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include <polkit/polkit.h>
#include <polkit-dbus/polkit-dbus.h>

#include "egg-debug.h"

#include "dkp-polkit.h"
#include "dkp-daemon.h"

#define DKP_POLKIT_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), DKP_TYPE_POLKIT, DkpPolkitPrivate))

struct DkpPolkitPrivate
{
	DBusGConnection		*connection;
	PolKitContext		*context;
	PolKitTracker		*tracker;
};

G_DEFINE_TYPE (DkpPolkit, dkp_polkit, G_TYPE_OBJECT)
static gpointer dkp_polkit_object = NULL;

/**
 * pk_polkit_io_watch_have_data:
 **/
static gboolean
pk_polkit_io_watch_have_data (GIOChannel *channel, GIOCondition condition, gpointer user_data)
{
	int fd;
	PolKitContext *context = user_data;
	fd = g_io_channel_unix_get_fd (channel);
	polkit_context_io_func (context, fd);
	return TRUE;
}

/**
 * pk_polkit_io_add_watch:
 **/
static int
pk_polkit_io_add_watch (PolKitContext *context, int fd)
{
	guint id = 0;
	GIOChannel *channel;
	channel = g_io_channel_unix_new (fd);
	if (channel == NULL)
		goto out;
	id = g_io_add_watch (channel, G_IO_IN, pk_polkit_io_watch_have_data, context);
	if (id == 0) {
		g_io_channel_unref (channel);
		goto out;
	}
	g_io_channel_unref (channel);
out:
	return id;
}

/**
 * pk_polkit_io_remove_watch:
 **/
static void
pk_polkit_io_remove_watch (PolKitContext *context, int watch_id)
{
	g_source_remove (watch_id);
}

/**
 * dkp_polkit_dbus_filter:
 **/
static DBusHandlerResult
dkp_polkit_dbus_filter (DBusConnection *connection, DBusMessage *message, void *user_data)
{
	DkpPolkit *polkit = DKP_POLKIT (user_data);
	const gchar *interface;

	interface = dbus_message_get_interface (message);

	/* pass NameOwnerChanged signals from the bus to PolKitTracker */
	if (dbus_message_is_signal (message, DBUS_INTERFACE_DBUS, "NameOwnerChanged"))
		polkit_tracker_dbus_func (polkit->priv->tracker, message);

	/* pass ConsoleKit signals to PolKitTracker */
	if (interface != NULL && g_str_has_prefix (interface, "org.freedesktop.ConsoleKit"))
		polkit_tracker_dbus_func (polkit->priv->tracker, message);

	/* other filters might want to process this message too */
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

/**
 * dkp_polkit_get_caller:
 **/
PolKitCaller *
dkp_polkit_get_caller (DkpPolkit *polkit, DBusGMethodInvocation *context)
{
	const gchar *sender;
	GError *error;
	DBusError dbus_error;
	PolKitCaller *caller;

	sender = dbus_g_method_get_sender (context);
	dbus_error_init (&dbus_error);
	caller = polkit_tracker_get_caller_from_dbus_name (polkit->priv->tracker, sender, &dbus_error);
	if (caller == NULL) {
		error = g_error_new (DKP_DAEMON_ERROR,
				     DKP_DAEMON_ERROR_GENERAL,
				     "Error getting information about caller: %s: %s",
				     dbus_error.name, dbus_error.message);
		dbus_error_free (&dbus_error);
		dbus_g_method_return_error (context, error);
		g_error_free (error);
		return NULL;
	}

	return caller;
}

/**
 * dkp_polkit_check_auth:
 **/
gboolean
dkp_polkit_check_auth (DkpPolkit *polkit, PolKitCaller *caller, const gchar *action_id, DBusGMethodInvocation *context)
{
	gboolean ret = FALSE;
	GError *error;
	DBusError dbus_error;
	PolKitAction *action;
	PolKitResult result;

	action = polkit_action_new ();
	polkit_action_set_action_id (action, action_id);
	result = polkit_context_is_caller_authorized (polkit->priv->context, action, caller, TRUE, NULL);
	if (result == POLKIT_RESULT_YES) {
		ret = TRUE;
	} else {
		dbus_error_init (&dbus_error);
		polkit_dbus_error_generate (action, result, &dbus_error);
		error = NULL;
		dbus_set_g_error (&error, &dbus_error);
		dbus_g_method_return_error (context, error);
		g_error_free (error);
		dbus_error_free (&dbus_error);
	}
	polkit_action_unref (action);
	return ret;
}

/**
 * dkp_polkit_finalize:
 **/
static void
dkp_polkit_finalize (GObject *object)
{
	DkpPolkit *polkit;
	g_return_if_fail (DKP_IS_POLKIT (object));
	polkit = DKP_POLKIT (object);

	if (polkit->priv->connection != NULL)
		dbus_g_connection_unref (polkit->priv->connection);
	if (polkit->priv->tracker != NULL)
		polkit_tracker_unref (polkit->priv->tracker);
	polkit_context_unref (polkit->priv->context);

	G_OBJECT_CLASS (dkp_polkit_parent_class)->finalize (object);
}

/**
 * dkp_polkit_class_init:
 **/
static void
dkp_polkit_class_init (DkpPolkitClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = dkp_polkit_finalize;
	g_type_class_add_private (klass, sizeof (DkpPolkitPrivate));
}

/**
 * dkp_polkit_init:
 *
 * initializes the polkit class. NOTE: We expect polkit objects
 * to *NOT* be removed or added during the session.
 * We only control the first polkit object if there are more than one.
 **/
static void
dkp_polkit_init (DkpPolkit *polkit)
{

	DBusConnection *connection;
	DBusError dbus_error;
	GError *error = NULL;

	polkit->priv = DKP_POLKIT_GET_PRIVATE (polkit);

	error = NULL;
	polkit->priv->connection = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
	if (polkit->priv->connection == NULL) {
		if (error != NULL) {
			g_critical ("error getting system bus: %s", error->message);
			g_error_free (error);
		}
		goto out;
	}
	connection = dbus_g_connection_get_connection (polkit->priv->connection);

	polkit->priv->context = polkit_context_new ();
	polkit_context_set_io_watch_functions (polkit->priv->context, pk_polkit_io_add_watch, pk_polkit_io_remove_watch);
	if (!polkit_context_init (polkit->priv->context, NULL)) {
		g_critical ("cannot initialize libpolkit");
		goto out;
	}

	polkit->priv->tracker = polkit_tracker_new ();
	polkit_tracker_set_system_bus_connection (polkit->priv->tracker, connection);
	polkit_tracker_init (polkit->priv->tracker);

	/* TODO FIXME: I'm pretty sure dbus-glib blows in a way that
	 * we can't say we're interested in all signals from all
	 * members on all interfaces for a given service... So we do
	 * this..
	 */

	dbus_error_init (&dbus_error);

	/* need to listen to NameOwnerChanged */
	dbus_bus_add_match (connection,
			    "type='signal'"
			    ",interface='"DBUS_INTERFACE_DBUS"'"
			    ",sender='"DBUS_SERVICE_DBUS"'"
			    ",member='NameOwnerChanged'",
			    &dbus_error);

	if (dbus_error_is_set (&dbus_error)) {
		egg_warning ("Cannot add match rule: %s: %s", dbus_error.name, dbus_error.message);
		dbus_error_free (&dbus_error);
		goto out;
	}

	/* need to listen to ConsoleKit signals */
	dbus_bus_add_match (connection,
			    "type='signal',sender='org.freedesktop.ConsoleKit'",
			    &dbus_error);

	if (dbus_error_is_set (&dbus_error)) {
		egg_warning ("Cannot add match rule: %s: %s", dbus_error.name, dbus_error.message);
		dbus_error_free (&dbus_error);
		goto out;
	}

	if (!dbus_connection_add_filter (connection, dkp_polkit_dbus_filter, polkit, NULL)) {
		egg_warning ("Cannot add D-Bus filter: %s: %s", dbus_error.name, dbus_error.message);
		goto out;
	}

out:
	return;
}

/**
 * dkp_polkit_new:
 * Return value: A new polkit class instance.
 **/
DkpPolkit *
dkp_polkit_new (void)
{
	if (dkp_polkit_object != NULL) {
		g_object_ref (dkp_polkit_object);
	} else {
		dkp_polkit_object = g_object_new (DKP_TYPE_POLKIT, NULL);
		g_object_add_weak_pointer (dkp_polkit_object, &dkp_polkit_object);
	}
	return DKP_POLKIT (dkp_polkit_object);
}

