/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 David Zeuthen <david@fubar.dk>
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

#include <string.h>

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <glib-object.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <devkit-gobject.h>

#include "dkp-debug.h"
#include "dkp-daemon.h"
#include "dkp-device.h"
#include "dkp-supply.h"
#include "dkp-csr.h"
#include "dkp-hid.h"
#include "dkp-device-list.h"

#include "dkp-daemon-glue.h"
#include "dkp-marshal.h"

enum
{
	DEVICE_ADDED_SIGNAL,
	DEVICE_REMOVED_SIGNAL,
	DEVICE_CHANGED_SIGNAL,
	ON_BATTERY_CHANGED_SIGNAL,
	LOW_BATTERY_CHANGED_SIGNAL,
	LAST_SIGNAL,
};

static const gchar *subsystems[] = {"power_supply", "usb", NULL};

static guint signals[LAST_SIGNAL] = { 0 };

struct DkpDaemonPrivate
{
	DBusGConnection		*system_bus_connection;
	DBusGProxy		*system_bus_proxy;
	PolKitContext		*pk_context;
	PolKitTracker		*pk_tracker;

	DkpDeviceList		*list;
	gboolean		 on_battery;
	gboolean		 low_battery;

	DevkitClient		*devkit_client;
};

static void	dkp_daemon_class_init	(DkpDaemonClass *klass);
static void	dkp_daemon_init		(DkpDaemon	*seat);
static void	dkp_daemon_finalize	(GObject	*object);
static gboolean	dkp_daemon_get_on_battery_local (DkpDaemon *daemon);
static gboolean	dkp_daemon_get_low_battery_local (DkpDaemon *daemon);

G_DEFINE_TYPE (DkpDaemon, dkp_daemon, G_TYPE_OBJECT)

#define DKP_DAEMON_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), DKP_TYPE_DAEMON, DkpDaemonPrivate))

/**
 * dkp_daemon_error_quark:
 **/
GQuark
dkp_daemon_error_quark (void)
{
	static GQuark ret = 0;
	if (ret == 0)
		ret = g_quark_from_static_string ("dkp_daemon_error");
	return ret;
}

#define ENUM_ENTRY(NAME, DESC) { NAME, "" #NAME "", DESC }
/**
 * dkp_daemon_error_get_type:
 **/
GType
dkp_daemon_error_get_type (void)
{
	static GType etype = 0;

	if (etype == 0)
	{
		static const GEnumValue values[] =
			{
				ENUM_ENTRY (DKP_DAEMON_ERROR_GENERAL, "GeneralError"),
				ENUM_ENTRY (DKP_DAEMON_ERROR_NOT_SUPPORTED, "NotSupported"),
				ENUM_ENTRY (DKP_DAEMON_ERROR_NO_SUCH_DEVICE, "NoSuchDevice"),
				{ 0, 0, 0 }
			};
		g_assert (DKP_DAEMON_NUM_ERRORS == G_N_ELEMENTS (values) - 1);
		etype = g_enum_register_static ("DkpDaemonError", values);
	}
	return etype;
}

/**
 * dkp_daemon_constructor:
 **/
static GObject *
dkp_daemon_constructor (GType type, guint n_construct_properties, GObjectConstructParam *construct_properties)
{
	DkpDaemon *daemon;
	DkpDaemonClass *klass;

	klass = DKP_DAEMON_CLASS (g_type_class_peek (DKP_TYPE_DAEMON));
	daemon = DKP_DAEMON (G_OBJECT_CLASS (dkp_daemon_parent_class)->constructor (type, n_construct_properties, construct_properties));
	return G_OBJECT (daemon);
}

/**
 * dkp_daemon_class_init:
 **/
static void
dkp_daemon_class_init (DkpDaemonClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->constructor = dkp_daemon_constructor;
	object_class->finalize = dkp_daemon_finalize;

	g_type_class_add_private (klass, sizeof (DkpDaemonPrivate));

	signals[DEVICE_ADDED_SIGNAL] =
		g_signal_new ("device-added",
			      G_OBJECT_CLASS_TYPE (klass),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1, G_TYPE_STRING);

	signals[DEVICE_REMOVED_SIGNAL] =
		g_signal_new ("device-removed",
			      G_OBJECT_CLASS_TYPE (klass),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1, G_TYPE_STRING);

	signals[DEVICE_CHANGED_SIGNAL] =
		g_signal_new ("device-changed",
			      G_OBJECT_CLASS_TYPE (klass),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1, G_TYPE_STRING);

	signals[ON_BATTERY_CHANGED_SIGNAL] =
		g_signal_new ("on-battery-changed",
			      G_OBJECT_CLASS_TYPE (klass),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__BOOLEAN,
			      G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

	signals[LOW_BATTERY_CHANGED_SIGNAL] =
		g_signal_new ("low-battery-changed",
			      G_OBJECT_CLASS_TYPE (klass),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__BOOLEAN,
			      G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

	dbus_g_object_type_install_info (DKP_TYPE_DAEMON, &dbus_glib_dkp_daemon_object_info);

	dbus_g_error_domain_register (DKP_DAEMON_ERROR, NULL, DKP_DAEMON_TYPE_ERROR);
}

/**
 * dkp_daemon_init:
 **/
static void
dkp_daemon_init (DkpDaemon *daemon)
{
	daemon->priv = DKP_DAEMON_GET_PRIVATE (daemon);
}

/**
 * dkp_daemon_finalize:
 **/
static void
dkp_daemon_finalize (GObject *object)
{
	DkpDaemon *daemon;

	g_return_if_fail (object != NULL);
	g_return_if_fail (DKP_IS_DAEMON (object));

	daemon = DKP_DAEMON (object);

	g_return_if_fail (daemon->priv != NULL);

	if (daemon->priv->pk_context != NULL)
		polkit_context_unref (daemon->priv->pk_context);

	if (daemon->priv->pk_tracker != NULL)
		polkit_tracker_unref (daemon->priv->pk_tracker);

	if (daemon->priv->system_bus_proxy != NULL)
		g_object_unref (daemon->priv->system_bus_proxy);

	if (daemon->priv->system_bus_connection != NULL)
		dbus_g_connection_unref (daemon->priv->system_bus_connection);

	if (daemon->priv->devkit_client != NULL)
		g_object_unref (daemon->priv->devkit_client);

	if (daemon->priv->list != NULL)
		g_object_unref (daemon->priv->list);

	G_OBJECT_CLASS (dkp_daemon_parent_class)->finalize (object);
}

/**
 * pk_io_watch_have_data:
 **/
static gboolean
pk_io_watch_have_data (GIOChannel *channel, GIOCondition condition, gpointer user_data)
{
	int fd;
	PolKitContext *pk_context = user_data;
	fd = g_io_channel_unix_get_fd (channel);
	polkit_context_io_func (pk_context, fd);
	return TRUE;
}

/**
 * pk_io_add_watch:
 **/
static int
pk_io_add_watch (PolKitContext *pk_context, int fd)
{
	guint id = 0;
	GIOChannel *channel;
	channel = g_io_channel_unix_new (fd);
	if (channel == NULL)
		goto out;
	id = g_io_add_watch (channel, G_IO_IN, pk_io_watch_have_data, pk_context);
	if (id == 0) {
		g_io_channel_unref (channel);
		goto out;
	}
	g_io_channel_unref (channel);
out:
	return id;
}

/**
 * pk_io_remove_watch:
 **/
static void
pk_io_remove_watch (PolKitContext *pk_context, int watch_id)
{
	g_source_remove (watch_id);
}

/**
 * gpk_daemon_dbus_filter:
 **/
static DBusHandlerResult
gpk_daemon_dbus_filter (DBusConnection *connection, DBusMessage *message, void *user_data)
{
	DkpDaemon *daemon = DKP_DAEMON (user_data);
	const gchar *interface;

	interface = dbus_message_get_interface (message);

	if (dbus_message_is_signal (message, DBUS_INTERFACE_DBUS, "NameOwnerChanged")) {
		/* pass NameOwnerChanged signals from the bus to PolKitTracker */
		polkit_tracker_dbus_func (daemon->priv->pk_tracker, message);
	}

	if (interface != NULL && g_str_has_prefix (interface, "org.freedesktop.ConsoleKit")) {
		/* pass ConsoleKit signals to PolKitTracker */
		polkit_tracker_dbus_func (daemon->priv->pk_tracker, message);
	}

	/* other filters might want to process this message too */
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static gboolean gpk_daemon_device_add (DkpDaemon *daemon, DevkitDevice *d, gboolean emit_event);
static void gpk_daemon_device_remove (DkpDaemon *daemon, DevkitDevice *d);

/**
 * dkp_daemon_get_on_battery_local:
 *
 * As soon as _any_ battery goes discharging, this is true
 **/
static gboolean
dkp_daemon_get_on_battery_local (DkpDaemon *daemon)
{
	guint i;
	gboolean ret;
	gboolean result = FALSE;
	gboolean on_battery;
	DkpDevice *device;
	const GPtrArray *array;

	/* ask each device */
	array = dkp_device_list_get_array (daemon->priv->list);
	for (i=0; i<array->len; i++) {
		device = (DkpDevice *) g_ptr_array_index (array, i);
		ret = dkp_device_get_on_battery (device, &on_battery);
		if (ret && on_battery) {
			result = TRUE;
			break;
		}
	}
	return result;
}

/**
 * dkp_daemon_get_low_battery_local:
 *
 * As soon as _all_ batteries are low, this is true
 **/
static gboolean
dkp_daemon_get_low_battery_local (DkpDaemon *daemon)
{
	guint i;
	gboolean ret;
	gboolean result = TRUE;
	gboolean low_battery;
	DkpDevice *device;
	const GPtrArray *array;

	/* ask each device */
	array = dkp_device_list_get_array (daemon->priv->list);
	for (i=0; i<array->len; i++) {
		device = (DkpDevice *) g_ptr_array_index (array, i);
		ret = dkp_device_get_low_battery (device, &low_battery);
		if (ret && !low_battery) {
			result = FALSE;
			break;
		}
	}
	return result;
}

/**
 * gpk_daemon_device_changed:
 **/
static void
gpk_daemon_device_changed (DkpDaemon *daemon, DevkitDevice *d, gboolean synthesized)
{
	DkpDevice *device;
	gboolean ret;

	/* check if the on_battery and low_battery state has changed */
	ret = dkp_daemon_get_on_battery_local (daemon);
	if (ret != daemon->priv->on_battery) {
		daemon->priv->on_battery = ret;
		dkp_debug ("now on_battery = %s", ret ? "yes" : "no");
		g_signal_emit (daemon, signals[ON_BATTERY_CHANGED_SIGNAL], 0, ret);
	}
	ret = dkp_daemon_get_low_battery_local (daemon);
	if (ret != daemon->priv->low_battery) {
		daemon->priv->low_battery = ret;
		dkp_debug ("now low_battery = %s", ret ? "yes" : "no");
		g_signal_emit (daemon, signals[LOW_BATTERY_CHANGED_SIGNAL], 0, ret);
	}

	/* does the device exist in the db? */
	device = dkp_device_list_lookup (daemon->priv->list, d);
	if (device != NULL) {
		dkp_debug ("changed %s", dkp_device_get_object_path (device));
		dkp_device_changed (device, d, synthesized);
	} else {
		dkp_debug ("treating change event as add on %s", dkp_device_get_object_path (device));
		gpk_daemon_device_add (daemon, d, TRUE);
	}
}

/**
 * gpk_daemon_device_went_away:
 **/
static void
gpk_daemon_device_went_away (gpointer user_data, GObject *_device)
{
	DkpDaemon *daemon = DKP_DAEMON (user_data);
	DkpDevice *device = DKP_DEVICE (_device);
	dkp_device_list_remove (daemon->priv->list, device);
}

/**
 * gpk_daemon_device_get:
 **/
static DkpDevice *
gpk_daemon_device_get (DkpDaemon *daemon, DevkitDevice *d)
{
	const gchar *subsys;
	const gchar *native_path;
	DkpDevice *device = NULL;
	gboolean ret;

	subsys = devkit_device_get_subsystem (d);
	if (strcmp (subsys, "power_supply") == 0) {
		/* always add */
		device = DKP_DEVICE (dkp_supply_new ());

	} else if (strcmp (subsys, "usb") == 0) {

		/* see if this is a CSR mouse or keyboard */
		device = DKP_DEVICE (dkp_csr_new ());
		ret = dkp_device_coldplug (device, daemon, d);
		if (ret)
			goto out;
		g_object_unref (device);

		/* try to detect a HID UPS */
		device = DKP_DEVICE (dkp_hid_new ());
		ret = dkp_device_coldplug (device, daemon, d);
		if (ret)
			goto out;
		g_object_unref (device);

		/* no valid USB object ;-( */
		device = NULL;

	} else {
		native_path = devkit_device_get_native_path (d);
		dkp_warning ("native path %s (%s) ignoring", native_path, subsys);
	}
out:
	return device;
}

/**
 * gpk_daemon_device_add:
 **/
static gboolean
gpk_daemon_device_add (DkpDaemon *daemon, DevkitDevice *d, gboolean emit_event)
{
	DkpDevice *device;
	gboolean ret = TRUE;

	/* does device exist in db? */
	device = dkp_device_list_lookup (daemon->priv->list, d);
	if (device != NULL) {
		/* we already have the device; treat as change event */
		dkp_debug ("treating add event as change event on %s", dkp_device_get_object_path (device));
		gpk_daemon_device_changed (daemon, d, FALSE);
	} else {

		/* get the right sort of device */
		device = gpk_daemon_device_get (daemon, d);
		if (device == NULL) {
			dkp_debug ("ignoring add event on %s", devkit_device_get_native_path (d));
			ret = FALSE;
			goto out;
		}
		/* only take a weak ref; the device will stay on the bus until
		 * it's unreffed. So if we ref it, it'll never go away.
		 */
		g_object_weak_ref (G_OBJECT (device), gpk_daemon_device_went_away, daemon);
		dkp_device_list_insert (daemon->priv->list, d, device);
		if (emit_event) {
			g_signal_emit (daemon, signals[DEVICE_ADDED_SIGNAL], 0,
				       dkp_device_get_object_path (device));
		}
	}
out:
	return ret;
}

/**
 * gpk_daemon_device_remove:
 **/
static void
gpk_daemon_device_remove (DkpDaemon *daemon, DevkitDevice *d)
{
	DkpDevice *device;

	/* does device exist in db? */
	device = dkp_device_list_lookup (daemon->priv->list, d);
	if (device == NULL) {
		dkp_debug ("ignoring remove event on %s", devkit_device_get_native_path (d));
	} else {
		dkp_device_removed (device);
		g_signal_emit (daemon, signals[DEVICE_REMOVED_SIGNAL], 0,
			       dkp_device_get_object_path (device));
		g_object_unref (device);
	}
}

/**
 * gpk_daemon_device_event_signal_handler:
 **/
static void
gpk_daemon_device_event_signal_handler (DevkitClient *client, const char *action,
					DevkitDevice *device, gpointer user_data)
{
	DkpDaemon *daemon = DKP_DAEMON (user_data);

	if (strcmp (action, "add") == 0) {
		dkp_debug ("add %s", devkit_device_get_native_path (device));
		gpk_daemon_device_add (daemon, device, TRUE);
	} else if (strcmp (action, "remove") == 0) {
		dkp_debug ("remove %s", devkit_device_get_native_path (device));
		gpk_daemon_device_remove (daemon, device);
	} else if (strcmp (action, "change") == 0) {
		dkp_debug ("change %s", devkit_device_get_native_path (device));
		gpk_daemon_device_changed (daemon, device, FALSE);
	} else {
		dkp_warning ("unhandled action '%s' on %s", action, devkit_device_get_native_path (device));
	}
}

/**
 * gpk_daemon_register_power_daemon:
 **/
static gboolean
gpk_daemon_register_power_daemon (DkpDaemon *daemon)
{
	DBusConnection *connection;
	DBusError dbus_error;
	GError *error = NULL;

	daemon->priv->pk_context = polkit_context_new ();
	polkit_context_set_io_watch_functions (daemon->priv->pk_context, pk_io_add_watch, pk_io_remove_watch);
	if (!polkit_context_init (daemon->priv->pk_context, NULL)) {
		g_critical ("cannot initialize libpolkit");
		goto error;
	}

	error = NULL;
	daemon->priv->system_bus_connection = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
	if (daemon->priv->system_bus_connection == NULL) {
		if (error != NULL) {
			g_critical ("error getting system bus: %s", error->message);
			g_error_free (error);
		}
		goto error;
	}
	connection = dbus_g_connection_get_connection (daemon->priv->system_bus_connection);

	daemon->priv->pk_tracker = polkit_tracker_new ();
	polkit_tracker_set_system_bus_connection (daemon->priv->pk_tracker, connection);
	polkit_tracker_init (daemon->priv->pk_tracker);

	dbus_g_connection_register_g_object (daemon->priv->system_bus_connection, "/",
					     G_OBJECT (daemon));

	daemon->priv->system_bus_proxy = dbus_g_proxy_new_for_name (daemon->priv->system_bus_connection,
								      DBUS_SERVICE_DBUS,
								      DBUS_PATH_DBUS,
								      DBUS_INTERFACE_DBUS);

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
		dkp_warning ("Cannot add match rule: %s: %s", dbus_error.name, dbus_error.message);
		dbus_error_free (&dbus_error);
		goto error;
	}

	/* need to listen to ConsoleKit signals */
	dbus_bus_add_match (connection,
			    "type='signal',sender='org.freedesktop.ConsoleKit'",
			    &dbus_error);

	if (dbus_error_is_set (&dbus_error)) {
		dkp_warning ("Cannot add match rule: %s: %s", dbus_error.name, dbus_error.message);
		dbus_error_free (&dbus_error);
		goto error;
	}

	if (!dbus_connection_add_filter (connection,
					 gpk_daemon_dbus_filter,
					 daemon,
					 NULL)) {
		dkp_warning ("Cannot add D-Bus filter: %s: %s", dbus_error.name, dbus_error.message);
		goto error;
	}

	/* connect to the DeviceKit daemon */
	daemon->priv->devkit_client = devkit_client_new (subsystems);
	if (!devkit_client_connect (daemon->priv->devkit_client, &error)) {
		dkp_warning ("Couldn't open connection to DeviceKit daemon: %s", error->message);
		g_error_free (error);
		goto error;
	}
	g_signal_connect (daemon->priv->devkit_client, "device-event",
			  G_CALLBACK (gpk_daemon_device_event_signal_handler), daemon);

	return TRUE;
error:
	return FALSE;
}

/**
 * dkp_daemon_new:
 **/
DkpDaemon *
dkp_daemon_new (void)
{
	DkpDaemon *daemon;
	GError *error = NULL;
	GList *devices;
	GList *l;

	daemon = DKP_DAEMON (g_object_new (DKP_TYPE_DAEMON, NULL));

	daemon->priv->list = dkp_device_list_new ();
	if (!gpk_daemon_register_power_daemon (DKP_DAEMON (daemon))) {
		g_object_unref (daemon);
		return NULL;
	}

	devices = devkit_client_enumerate_by_subsystem (daemon->priv->devkit_client, subsystems, &error);
	if (error != NULL) {
		dkp_warning ("Cannot enumerate devices: %s", error->message);
		g_error_free (error);
		g_object_unref (daemon);
		return NULL;
	}

	for (l = devices; l != NULL; l = l->next) {
		DevkitDevice *device = l->data;
		gpk_daemon_device_add (daemon, device, FALSE);
	}
	g_list_foreach (devices, (GFunc) g_object_unref, NULL);
	g_list_free (devices);

	daemon->priv->on_battery = dkp_daemon_get_on_battery_local (daemon);
	daemon->priv->low_battery = dkp_daemon_get_low_battery_local (daemon);

	return daemon;
}

/**
 * dkp_daemon_local_get_caller_for_context:
 **/
PolKitCaller *
dkp_daemon_local_get_caller_for_context (DkpDaemon *daemon, DBusGMethodInvocation *context)
{
	const gchar *sender;
	GError *error;
	DBusError dbus_error;
	PolKitCaller *pk_caller;

	sender = dbus_g_method_get_sender (context);
	dbus_error_init (&dbus_error);
	pk_caller = polkit_tracker_get_caller_from_dbus_name (daemon->priv->pk_tracker,
							      sender,
							      &dbus_error);
	if (pk_caller == NULL) {
		error = g_error_new (DKP_DAEMON_ERROR,
				     DKP_DAEMON_ERROR_GENERAL,
				     "Error getting information about caller: %s: %s",
				     dbus_error.name, dbus_error.message);
		dbus_error_free (&dbus_error);
		dbus_g_method_return_error (context, error);
		g_error_free (error);
		return NULL;
	}

	return pk_caller;
}

/**
 * dkp_daemon_local_check_auth:
 **/
gboolean
dkp_daemon_local_check_auth (DkpDaemon *daemon, PolKitCaller *pk_caller, const char *action_id, DBusGMethodInvocation *context)
{
	gboolean ret = FALSE;
	GError *error;
	DBusError d_error;
	PolKitAction *pk_action;
	PolKitResult pk_result;

	pk_action = polkit_action_new ();
	polkit_action_set_action_id (pk_action, action_id);
	pk_result = polkit_context_is_caller_authorized (daemon->priv->pk_context, pk_action, pk_caller, TRUE, NULL);
	if (pk_result == POLKIT_RESULT_YES) {
		ret = TRUE;
	} else {
		dbus_error_init (&d_error);
		polkit_dbus_error_generate (pk_action, pk_result, &d_error);
		error = NULL;
		dbus_set_g_error (&error, &d_error);
		dbus_g_method_return_error (context, error);
		g_error_free (error);
		dbus_error_free (&d_error);
	}
	polkit_action_unref (pk_action);
	return ret;
}

#if 0
/**
 * gpk_daemon_throw_error:
 **/
static gboolean
gpk_daemon_throw_error (DBusGMethodInvocation *context, int error_code, const char *format, ...)
{
	GError *error;
	va_list args;
	gchar *message;

	va_start (args, format);
	message = g_strdup_vprintf (format, args);
	va_end (args);

	error = g_error_new (DKP_DAEMON_ERROR,
			     error_code,
			     message);
	dbus_g_method_return_error (context, error);
	g_error_free (error);
	g_free (message);
	return TRUE;
}
#endif

/* exported methods */

/**
 * dkp_daemon_enumerate_devices:
 **/
gboolean
dkp_daemon_enumerate_devices (DkpDaemon *daemon, DBusGMethodInvocation *context)
{
	guint i;
	const GPtrArray *array;
	GPtrArray *object_paths;
	DkpDevice *device;

	/* build a pointer array of the object paths */
	object_paths = g_ptr_array_new ();
	array = dkp_device_list_get_array (daemon->priv->list);
	for (i=0; i<array->len; i++) {
		device = (DkpDevice *) g_ptr_array_index (array, i);
		g_ptr_array_add (object_paths, g_strdup (dkp_device_get_object_path (device)));
	}

	/* return it on the bus */
	dbus_g_method_return (context, object_paths);

	/* free */
	g_ptr_array_foreach (object_paths, (GFunc) g_free, NULL);
	g_ptr_array_free (object_paths, TRUE);
	return TRUE;
}

/**
 * dkp_daemon_get_on_battery:
 **/
gboolean
dkp_daemon_get_on_battery (DkpDaemon *daemon, DBusGMethodInvocation *context)
{
	/* this is cached as it's expensive to check all sources */
	dbus_g_method_return (context, daemon->priv->on_battery);
	return TRUE;
}

/**
 * dkp_daemon_get_low_battery:
 **/
gboolean
dkp_daemon_get_low_battery (DkpDaemon *daemon, DBusGMethodInvocation *context)
{
	/* this is cached as it's expensive to check all sources */
	dbus_g_method_return (context, daemon->priv->low_battery);
	return TRUE;
}

/**
 * dkp_daemon_suspend:
 **/
gboolean
dkp_daemon_suspend (DkpDaemon *daemon, DBusGMethodInvocation *context)
{
	gboolean ret;
	GError *error;
	GError *error_local = NULL;
	gchar *argv;
	const gchar *quirks;
	PolKitCaller *pk_caller;

	pk_caller = dkp_daemon_local_get_caller_for_context (daemon, context);
	if (pk_caller == NULL)
		goto out;

	if (!dkp_daemon_local_check_auth (daemon, pk_caller, "org.freedesktop.devicekit.power.suspend", context))
		goto out;

	/* TODO: where from? */
	quirks = "--quirk-s3-bios --quirk-s3-mode";

	argv = g_strdup_printf ("/usr/sbin/pm-suspend %s", quirks);
	ret = g_spawn_command_line_async (argv, &error_local);
	if (!ret) {
		error = g_error_new (DKP_DAEMON_ERROR,
				     DKP_DAEMON_ERROR_GENERAL,
				     "Cannot spawn: %s", error_local->message);
		g_error_free (error_local);
		dbus_g_method_return_error (context, error);
		goto out;
	}
	dbus_g_method_return (context, NULL);
out:
	if (pk_caller != NULL)
		polkit_caller_unref (pk_caller);
	return TRUE;
}

/**
 * dkp_daemon_hibernate:
 **/
gboolean
dkp_daemon_hibernate (DkpDaemon *daemon, DBusGMethodInvocation *context)
{
	gboolean ret;
	GError *error;
	GError *error_local = NULL;
	gchar *argv;
	const gchar *quirks;
	PolKitCaller *pk_caller;

	pk_caller = dkp_daemon_local_get_caller_for_context (daemon, context);
	if (pk_caller == NULL)
		goto out;

	if (!dkp_daemon_local_check_auth (daemon, pk_caller, "org.freedesktop.devicekit.power.hibernate", context))
		goto out;

	/* TODO: where from? */
	quirks = "--quirk-s3-bios --quirk-s3-mode";

	argv = g_strdup_printf ("/usr/sbin/pm-hibernate %s", quirks);
	ret = g_spawn_command_line_async (argv, &error_local);
	if (!ret) {
		error = g_error_new (DKP_DAEMON_ERROR,
				     DKP_DAEMON_ERROR_GENERAL,
				     "Cannot spawn: %s", error_local->message);
		g_error_free (error_local);
		dbus_g_method_return_error (context, error);
		goto out;
	}
	dbus_g_method_return (context, NULL);
out:
	if (pk_caller != NULL)
		polkit_caller_unref (pk_caller);
	return TRUE;
}

