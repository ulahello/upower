/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
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

#include "devkit-power-daemon.h"
#include "devkit-power-device.h"

#include "devkit-power-daemon-glue.h"
#include "devkit-power-marshal.h"

/*--------------------------------------------------------------------------------------------------------------*/

enum
{
        DEVICE_ADDED_SIGNAL,
        DEVICE_REMOVED_SIGNAL,
        DEVICE_CHANGED_SIGNAL,
        ON_BATTERY_CHANGED_SIGNAL,
        LOW_BATTERY_CHANGED_SIGNAL,
        LAST_SIGNAL,
};

static guint signals[LAST_SIGNAL] = { 0 };

struct DevkitPowerDaemonPrivate
{
        DBusGConnection   *system_bus_connection;
        DBusGProxy        *system_bus_proxy;
        PolKitContext     *pk_context;
        PolKitTracker     *pk_tracker;

        GHashTable        *map_native_path_to_device;
        gboolean           on_battery;
        gboolean           low_battery;

        DevkitClient      *devkit_client;
};

static void     devkit_power_daemon_class_init  (DevkitPowerDaemonClass *klass);
static void     devkit_power_daemon_init        (DevkitPowerDaemon      *seat);
static void     devkit_power_daemon_finalize    (GObject     *object);

G_DEFINE_TYPE (DevkitPowerDaemon, devkit_power_daemon, G_TYPE_OBJECT)

#define DEVKIT_POWER_DAEMON_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), DEVKIT_TYPE_POWER_DAEMON, DevkitPowerDaemonPrivate))

/*--------------------------------------------------------------------------------------------------------------*/

GQuark
devkit_power_daemon_error_quark (void)
{
        static GQuark ret = 0;

        if (ret == 0) {
                ret = g_quark_from_static_string ("devkit_power_daemon_error");
        }

        return ret;
}


#define ENUM_ENTRY(NAME, DESC) { NAME, "" #NAME "", DESC }

GType
devkit_power_daemon_error_get_type (void)
{
        static GType etype = 0;

        if (etype == 0)
        {
                static const GEnumValue values[] =
                        {
                                ENUM_ENTRY (DEVKIT_POWER_DAEMON_ERROR_GENERAL, "GeneralError"),
                                ENUM_ENTRY (DEVKIT_POWER_DAEMON_ERROR_NOT_SUPPORTED, "NotSupported"),
                                ENUM_ENTRY (DEVKIT_POWER_DAEMON_ERROR_NO_SUCH_DEVICE, "NoSuchDevice"),
                                { 0, 0, 0 }
                        };
                g_assert (DEVKIT_POWER_DAEMON_NUM_ERRORS == G_N_ELEMENTS (values) - 1);
                etype = g_enum_register_static ("DevkitPowerDaemonError", values);
        }
        return etype;
}


static GObject *
devkit_power_daemon_constructor (GType                  type,
                                 guint                  n_construct_properties,
                                 GObjectConstructParam *construct_properties)
{
        DevkitPowerDaemon      *daemon;
        DevkitPowerDaemonClass *klass;

        klass = DEVKIT_POWER_DAEMON_CLASS (g_type_class_peek (DEVKIT_TYPE_POWER_DAEMON));

        daemon = DEVKIT_POWER_DAEMON (
                G_OBJECT_CLASS (devkit_power_daemon_parent_class)->constructor (type,
                                                                                n_construct_properties,
                                                                                construct_properties));
        return G_OBJECT (daemon);
}

static void
devkit_power_daemon_class_init (DevkitPowerDaemonClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);

        object_class->constructor = devkit_power_daemon_constructor;
        object_class->finalize = devkit_power_daemon_finalize;

        g_type_class_add_private (klass, sizeof (DevkitPowerDaemonPrivate));

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

        dbus_g_object_type_install_info (DEVKIT_TYPE_POWER_DAEMON, &dbus_glib_devkit_power_daemon_object_info);

        dbus_g_error_domain_register (DEVKIT_POWER_DAEMON_ERROR,
                                      NULL,
                                      DEVKIT_POWER_DAEMON_TYPE_ERROR);
}

static void
devkit_power_daemon_init (DevkitPowerDaemon *daemon)
{
        daemon->priv = DEVKIT_POWER_DAEMON_GET_PRIVATE (daemon);
        daemon->priv->on_battery = FALSE;
        daemon->priv->low_battery = FALSE;
        daemon->priv->map_native_path_to_device = g_hash_table_new_full (g_str_hash,
                                                                         g_str_equal,
                                                                         g_free,
                                                                         NULL);
}

static void
devkit_power_daemon_finalize (GObject *object)
{
        DevkitPowerDaemon *daemon;

        g_return_if_fail (object != NULL);
        g_return_if_fail (DEVKIT_IS_POWER_DAEMON (object));

        daemon = DEVKIT_POWER_DAEMON (object);

        g_return_if_fail (daemon->priv != NULL);

        if (daemon->priv->pk_context != NULL)
                polkit_context_unref (daemon->priv->pk_context);

        if (daemon->priv->pk_tracker != NULL)
                polkit_tracker_unref (daemon->priv->pk_tracker);

        if (daemon->priv->system_bus_proxy != NULL)
                g_object_unref (daemon->priv->system_bus_proxy);

        if (daemon->priv->system_bus_connection != NULL)
                dbus_g_connection_unref (daemon->priv->system_bus_connection);

        if (daemon->priv->devkit_client != NULL) {
                g_object_unref (daemon->priv->devkit_client);
        }

        if (daemon->priv->map_native_path_to_device != NULL) {
                g_hash_table_unref (daemon->priv->map_native_path_to_device);
        }

        G_OBJECT_CLASS (devkit_power_daemon_parent_class)->finalize (object);
}

static gboolean
pk_io_watch_have_data (GIOChannel *channel, GIOCondition condition, gpointer user_data)
{
        int fd;
        PolKitContext *pk_context = user_data;
        fd = g_io_channel_unix_get_fd (channel);
        polkit_context_io_func (pk_context, fd);
        return TRUE;
}

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

static void
pk_io_remove_watch (PolKitContext *pk_context, int watch_id)
{
        g_source_remove (watch_id);
}

static DBusHandlerResult
_filter (DBusConnection *connection, DBusMessage *message, void *user_data)
{
        DevkitPowerDaemon *daemon = DEVKIT_POWER_DAEMON (user_data);
        const char *interface;

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

static void device_add (DevkitPowerDaemon *daemon, DevkitDevice *d, gboolean emit_event);
static void device_remove (DevkitPowerDaemon *daemon, DevkitDevice *d);

static void
device_changed (DevkitPowerDaemon *daemon, DevkitDevice *d, gboolean synthesized)
{
        DevkitPowerDevice *device;
        const char *native_path;

        native_path = devkit_device_get_native_path (d);
        device = g_hash_table_lookup (daemon->priv->map_native_path_to_device, native_path);
        if (device != NULL) {
                if (!devkit_power_device_changed (device, d, synthesized)) {
                        g_print ("changed triggered remove on %s\n", native_path);
                        device_remove (daemon, d);
                } else {
                        g_print ("changed %s\n", native_path);
                }
        } else {
                g_print ("treating change event as add on %s\n", native_path);
                device_add (daemon, d, TRUE);
        }
}

static gboolean
device_went_away_remove_cb (gpointer key, gpointer value, gpointer user_data)
{
        if (value == user_data) {
                g_print ("removed %s\n", (char *) key);
                return TRUE;
        }
        return FALSE;
}

static void
device_went_away (gpointer user_data, GObject *where_the_object_was)
{
        DevkitPowerDaemon *daemon = DEVKIT_POWER_DAEMON (user_data);

        g_hash_table_foreach_remove (daemon->priv->map_native_path_to_device,
                                     device_went_away_remove_cb,
                                     where_the_object_was);
}

static void
device_add (DevkitPowerDaemon *daemon, DevkitDevice *d, gboolean emit_event)
{
        DevkitPowerDevice *device;
        const char *native_path;

        native_path = devkit_device_get_native_path (d);
        device = g_hash_table_lookup (daemon->priv->map_native_path_to_device, native_path);
        if (device != NULL) {
                /* we already have the device; treat as change event */
                g_print ("treating add event as change event on %s\n", native_path);
                device_changed (daemon, d, FALSE);
        } else {
                device = devkit_power_device_new (daemon, d);

                if (device != NULL) {
                        /* only take a weak ref; the device will stay on the bus until
                         * it's unreffed. So if we ref it, it'll never go away. Stupid
                         * dbus-glib, no cookie for you.
                         */
                        g_object_weak_ref (G_OBJECT (device), device_went_away, daemon);
                        g_hash_table_insert (daemon->priv->map_native_path_to_device,
                                             g_strdup (native_path),
                                             device);
                        g_print ("added %s\n", native_path);
                        if (emit_event) {
                                g_signal_emit (daemon, signals[DEVICE_ADDED_SIGNAL], 0,
                                               devkit_power_device_get_object_path (device));
                        }
                } else {
                        g_print ("ignoring add event on %s\n", native_path);
                }
        }
}

static void
device_remove (DevkitPowerDaemon *daemon, DevkitDevice *d)
{
        DevkitPowerDevice *device;
        const char *native_path;

        native_path = devkit_device_get_native_path (d);
        device = g_hash_table_lookup (daemon->priv->map_native_path_to_device, native_path);
        if (device == NULL) {
                g_print ("ignoring remove event on %s\n", native_path);
        } else {
                devkit_power_device_removed (device);
                g_signal_emit (daemon, signals[DEVICE_REMOVED_SIGNAL], 0,
                               devkit_power_device_get_object_path (device));
                g_object_unref (device);
        }
}

//TODO: hook into the devices
//g_signal_emit (daemon, signals[ON_BATTERY_CHANGED_SIGNAL], 0, FALSE);
//g_signal_emit (daemon, signals[LOW_BATTERY_CHANGED_SIGNAL], 0, FALSE);

static void
device_event_signal_handler (DevkitClient *client,
                             const char   *action,
                             DevkitDevice *device,
                             gpointer      user_data)
{
        DevkitPowerDaemon *daemon = DEVKIT_POWER_DAEMON (user_data);

        if (strcmp (action, "add") == 0) {
                device_add (daemon, device, TRUE);
        } else if (strcmp (action, "remove") == 0) {
                device_remove (daemon, device);
        } else if (strcmp (action, "change") == 0) {
                device_changed (daemon, device, FALSE);
        } else {
                g_warning ("unhandled action '%s' on %s", action, devkit_device_get_native_path (device));
        }
}

static gboolean
register_power_daemon (DevkitPowerDaemon *daemon)
{
        DBusConnection *connection;
        DBusError dbus_error;
        GError *error = NULL;
        const char *subsystems[] = {"power_supply", NULL};

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
                g_warning ("Cannot add match rule: %s: %s", dbus_error.name, dbus_error.message);
                dbus_error_free (&dbus_error);
                goto error;
        }

        /* need to listen to ConsoleKit signals */
	dbus_bus_add_match (connection,
			    "type='signal',sender='org.freedesktop.ConsoleKit'",
			    &dbus_error);

        if (dbus_error_is_set (&dbus_error)) {
                g_warning ("Cannot add match rule: %s: %s", dbus_error.name, dbus_error.message);
                dbus_error_free (&dbus_error);
                goto error;
        }

        if (!dbus_connection_add_filter (connection,
                                         _filter,
                                         daemon,
                                         NULL)) {
                g_warning ("Cannot add D-Bus filter: %s: %s", dbus_error.name, dbus_error.message);
                goto error;
        }

        /* connect to the DeviceKit daemon */
        daemon->priv->devkit_client = devkit_client_new (subsystems);
        if (!devkit_client_connect (daemon->priv->devkit_client, &error)) {
		g_warning ("Couldn't open connection to DeviceKit daemon: %s", error->message);
                g_error_free (error);
                goto error;
        }
        g_signal_connect (daemon->priv->devkit_client, "device-event",
                          G_CALLBACK (device_event_signal_handler), daemon);

        return TRUE;
error:
        return FALSE;
}


DevkitPowerDaemon *
devkit_power_daemon_new (void)
{
        DevkitPowerDaemon *daemon;
        GError *error = NULL;
        GList *devices;
        GList *l;
        const char *subsystems[] = {"power_supply", NULL};

        daemon = DEVKIT_POWER_DAEMON (g_object_new (DEVKIT_TYPE_POWER_DAEMON, NULL));

        if (!register_power_daemon (DEVKIT_POWER_DAEMON (daemon))) {
                g_object_unref (daemon);
                return NULL;
        }


        devices = devkit_client_enumerate_by_subsystem (daemon->priv->devkit_client,
                                                         subsystems,
                                                         &error);
        if (error != NULL) {
                g_warning ("Cannot enumerate devices: %s", error->message);
                g_error_free (error);
                g_object_unref (daemon);
                return NULL;
        }
        for (l = devices; l != NULL; l = l->next) {
                DevkitDevice *device = l->data;
                device_add (daemon, device, FALSE);
        }
        g_list_foreach (devices, (GFunc) g_object_unref, NULL);
        g_list_free (devices);

        return daemon;
}

PolKitCaller *
devkit_power_damon_local_get_caller_for_context (DevkitPowerDaemon *daemon,
                                                 DBusGMethodInvocation *context)
{
        const char *sender;
        GError *error;
        DBusError dbus_error;
        PolKitCaller *pk_caller;

        sender = dbus_g_method_get_sender (context);
        dbus_error_init (&dbus_error);
        pk_caller = polkit_tracker_get_caller_from_dbus_name (daemon->priv->pk_tracker,
                                                              sender,
                                                              &dbus_error);
        if (pk_caller == NULL) {
                error = g_error_new (DEVKIT_POWER_DAEMON_ERROR,
                                     DEVKIT_POWER_DAEMON_ERROR_GENERAL,
                                     "Error getting information about caller: %s: %s",
                                     dbus_error.name, dbus_error.message);
                dbus_error_free (&dbus_error);
                dbus_g_method_return_error (context, error);
                g_error_free (error);
                return NULL;
        }

        return pk_caller;
}

gboolean
devkit_power_damon_local_check_auth (DevkitPowerDaemon     *daemon,
                                     PolKitCaller          *pk_caller,
                                     const char            *action_id,
                                     DBusGMethodInvocation *context)
{
        gboolean ret;
        GError *error;
        DBusError d_error;
        PolKitAction *pk_action;
        PolKitResult pk_result;

        ret = FALSE;

        pk_action = polkit_action_new ();
        polkit_action_set_action_id (pk_action, action_id);
        pk_result = polkit_context_is_caller_authorized (daemon->priv->pk_context,
                                                         pk_action,
                                                         pk_caller,
                                                         TRUE,
                                                         NULL);
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


/*--------------------------------------------------------------------------------------------------------------*/

#if 0
static gboolean
throw_error (DBusGMethodInvocation *context, int error_code, const char *format, ...)
{
        GError *error;
        va_list args;
        char *message;

        va_start (args, format);
        message = g_strdup_vprintf (format, args);
        va_end (args);

        error = g_error_new (DEVKIT_POWER_DAEMON_ERROR,
                             error_code,
                             message);
        dbus_g_method_return_error (context, error);
        g_error_free (error);
        g_free (message);
        return TRUE;
}
#endif

/*--------------------------------------------------------------------------------------------------------------*/
/* exported methods */

static void
enumerate_cb (gpointer key, gpointer value, gpointer user_data)
{
        DevkitPowerDevice *device = DEVKIT_POWER_DEVICE (value);
        GPtrArray *object_paths = user_data;
        g_ptr_array_add (object_paths, g_strdup (devkit_power_device_get_object_path (device)));
}

gboolean
devkit_power_daemon_enumerate_devices (DevkitPowerDaemon     *daemon,
                                       DBusGMethodInvocation *context)
{
        GPtrArray *object_paths;
        object_paths = g_ptr_array_new ();
        g_hash_table_foreach (daemon->priv->map_native_path_to_device, enumerate_cb, object_paths);
        dbus_g_method_return (context, object_paths);
        g_ptr_array_foreach (object_paths, (GFunc) g_free, NULL);
        g_ptr_array_free (object_paths, TRUE);
        return TRUE;
}

gboolean
devkit_power_daemon_get_on_battery (DevkitPowerDaemon     *daemon,
                                    DBusGMethodInvocation *context)
{
        /* this is cached as it's expensive to check all sources */
        dbus_g_method_return (context, daemon->priv->on_battery);
        return TRUE;
}

gboolean
devkit_power_daemon_get_low_battery (DevkitPowerDaemon     *daemon,
                                     DBusGMethodInvocation *context)
{
        /* this is cached as it's expensive to check all sources */
        dbus_g_method_return (context, daemon->priv->low_battery);
        return TRUE;
}

gboolean
devkit_power_daemon_suspend (DevkitPowerDaemon *daemon, DBusGMethodInvocation *context)
{
        gboolean ret;
        GError *error;
        GError *error_local = NULL;
        gchar *argv;
        const gchar *quirks;
        PolKitCaller *pk_caller;

        pk_caller = devkit_power_damon_local_get_caller_for_context (daemon, context);
        if (pk_caller == NULL)
                goto out;

        if (!devkit_power_damon_local_check_auth (daemon, pk_caller,
                                                  "org.freedesktop.devicekit.power.suspend",
                                                  context))
                goto out;

        /* TODO: where from? */
        quirks = "--quirk-s3-bios --quirk-s3-mode";

        argv = g_strdup_printf ("/usr/sbin/pm-suspend %s", quirks);
        ret = g_spawn_command_line_async (argv, &error_local);
        if (!ret) {
                error = g_error_new (DEVKIT_POWER_DAEMON_ERROR,
                                     DEVKIT_POWER_DAEMON_ERROR_GENERAL,
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

gboolean
devkit_power_daemon_hibernate (DevkitPowerDaemon *daemon, DBusGMethodInvocation *context)
{
        gboolean ret;
        GError *error;
        GError *error_local = NULL;
        gchar *argv;
        const gchar *quirks;
        PolKitCaller *pk_caller;

        pk_caller = devkit_power_damon_local_get_caller_for_context (daemon, context);
        if (pk_caller == NULL)
                goto out;

        if (!devkit_power_damon_local_check_auth (daemon, pk_caller,
                                                  "org.freedesktop.devicekit.power.hibernate",
                                                  context))
                goto out;

        /* TODO: where from? */
        quirks = "--quirk-s3-bios --quirk-s3-mode";

        argv = g_strdup_printf ("/usr/sbin/pm-hibernate %s", quirks);
        ret = g_spawn_command_line_async (argv, &error_local);
        if (!ret) {
                error = g_error_new (DEVKIT_POWER_DAEMON_ERROR,
                                     DEVKIT_POWER_DAEMON_ERROR_GENERAL,
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
