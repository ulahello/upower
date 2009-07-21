/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 David Zeuthen <davidz@redhat.com>
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

#include <string.h>
#include <stdlib.h>

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <glib-object.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <gudev/gudev.h>

#include "egg-debug.h"

#include "dkp-polkit.h"
#include "dkp-daemon.h"
#include "dkp-device.h"
#include "dkp-device-supply.h"
#include "dkp-device-csr.h"
#include "dkp-device-wup.h"
#include "dkp-device-hid.h"
#include "dkp-input.h"
#include "dkp-device-list.h"

#include "dkp-daemon-glue.h"
#include "dkp-marshal.h"

enum
{
	PROP_0,
	PROP_DAEMON_VERSION,
	PROP_CAN_SUSPEND,
	PROP_CAN_HIBERNATE,
	PROP_ON_BATTERY,
	PROP_ON_LOW_BATTERY,
	PROP_LID_IS_CLOSED,
	PROP_LID_IS_PRESENT,
	PROP_LAST
};

enum
{
	DEVICE_ADDED_SIGNAL,
	DEVICE_REMOVED_SIGNAL,
	DEVICE_CHANGED_SIGNAL,
	CHANGED_SIGNAL,
	LAST_SIGNAL,
};

static const gchar *subsystems[] = {"power_supply", "usb", "tty", "input", NULL};

static guint signals[LAST_SIGNAL] = { 0 };

struct DkpDaemonPrivate
{
	DBusGConnection		*connection;
	DBusGProxy		*proxy;
	DkpPolkit		*polkit;
	DkpDeviceList		*power_devices;
	DkpDeviceList		*managed_devices;
	gboolean		 on_battery;
	gboolean		 low_battery;
	GUdevClient		*gudev_client;
	gboolean		 lid_is_closed;
	gboolean		 lid_is_present;
	gboolean		 can_suspend;
	gboolean		 can_hibernate;
};

static void	dkp_daemon_class_init		(DkpDaemonClass *klass);
static void	dkp_daemon_init			(DkpDaemon	*seat);
static void	dkp_daemon_finalize		(GObject	*object);
static gboolean	dkp_daemon_get_on_battery_local	(DkpDaemon	*daemon);
static gboolean	dkp_daemon_get_low_battery_local (DkpDaemon	*daemon);
static gboolean	dkp_daemon_get_on_ac_local 	(DkpDaemon	*daemon);

G_DEFINE_TYPE (DkpDaemon, dkp_daemon, G_TYPE_OBJECT)

#define DKP_DAEMON_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), DKP_TYPE_DAEMON, DkpDaemonPrivate))

/* if using more memory compared to usable swap, disable hibernate */
#define DKP_DAEMON_SWAP_WATERLINE 			80.0f /* % */

/* refresh all the devices after this much time when on-battery has changed */
#define DKP_DAEMON_ON_BATTERY_REFRESH_DEVICES_DELAY	3 /* seconds */

/**
 * dkp_daemon_set_lid_is_closed:
 **/
gboolean
dkp_daemon_set_lid_is_closed (DkpDaemon *daemon, gboolean lid_is_closed, gboolean notify)
{
	gboolean ret = FALSE;

	g_return_val_if_fail (DKP_IS_DAEMON (daemon), FALSE);

	egg_debug ("lid_is_closed=%i", lid_is_closed);
	if (daemon->priv->lid_is_closed == lid_is_closed) {
		egg_debug ("ignoring duplicate");
		goto out;
	}

	/* save */
	if (!notify) {
		/* Do not emit an event on startup. Otherwise, e. g.
		 * gnome-power-manager would pick up a "lid is closed" change
		 * event when dk-p gets D-BUS activated, and thus would
		 * immediately suspend the machine on startup. FD#22574 */
		egg_debug ("not emitting lid change event for daemon startup");
	} else {
		g_signal_emit (daemon, signals[CHANGED_SIGNAL], 0);
	}
	daemon->priv->lid_is_closed = lid_is_closed;
	ret = TRUE;
out:
	return ret;
}

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
 * dkp_daemon_get_property:
 **/
static void
dkp_daemon_get_property (GObject         *object,
			 guint            prop_id,
			 GValue          *value,
			 GParamSpec      *pspec)
{
	DkpDaemon *daemon;

	daemon = DKP_DAEMON (object);

	switch (prop_id) {

	case PROP_DAEMON_VERSION:
		g_value_set_string (value, PACKAGE_VERSION);
		break;

	case PROP_CAN_SUSPEND:
		g_value_set_boolean (value, daemon->priv->can_suspend);
		break;

	case PROP_CAN_HIBERNATE:
		g_value_set_boolean (value, daemon->priv->can_hibernate);
		break;

	case PROP_ON_BATTERY:
		g_value_set_boolean (value, daemon->priv->on_battery);
		break;

	case PROP_ON_LOW_BATTERY:
		g_value_set_boolean (value, daemon->priv->on_battery && daemon->priv->low_battery);
		break;

	case PROP_LID_IS_CLOSED:
		g_value_set_boolean (value, daemon->priv->lid_is_closed);
		break;

	case PROP_LID_IS_PRESENT:
		g_value_set_boolean (value, daemon->priv->lid_is_present);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
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
	object_class->get_property = dkp_daemon_get_property;

	g_type_class_add_private (klass, sizeof (DkpDaemonPrivate));

	signals[DEVICE_ADDED_SIGNAL] =
		g_signal_new ("device-added",
			      G_OBJECT_CLASS_TYPE (klass),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
			      0, NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1, G_TYPE_STRING);

	signals[DEVICE_REMOVED_SIGNAL] =
		g_signal_new ("device-removed",
			      G_OBJECT_CLASS_TYPE (klass),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
			      0, NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1, G_TYPE_STRING);

	signals[DEVICE_CHANGED_SIGNAL] =
		g_signal_new ("device-changed",
			      G_OBJECT_CLASS_TYPE (klass),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
			      0, NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1, G_TYPE_STRING);

	signals[CHANGED_SIGNAL] =
		g_signal_new ("changed",
			      G_OBJECT_CLASS_TYPE (klass),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
			      0, NULL, NULL,
			      g_cclosure_marshal_VOID__BOOLEAN,
			      G_TYPE_NONE, 0);


	g_object_class_install_property (object_class,
					 PROP_DAEMON_VERSION,
					 g_param_spec_string ("daemon-version",
							      "Daemon Version",
							      "The version of the running daemon",
							      NULL,
							      G_PARAM_READABLE));

	g_object_class_install_property (object_class,
					 PROP_LID_IS_PRESENT,
					 g_param_spec_boolean ("lid-is-present",
							       "Is a laptop",
							       "If this computer is probably a laptop",
							       FALSE,
							       G_PARAM_READABLE));

	g_object_class_install_property (object_class,
					 PROP_CAN_SUSPEND,
					 g_param_spec_boolean ("can-suspend",
							       "Can Suspend",
							       "Whether the system can suspend",
							       FALSE,
							       G_PARAM_READABLE));

	g_object_class_install_property (object_class,
					 PROP_CAN_HIBERNATE,
					 g_param_spec_boolean ("can-hibernate",
							       "Can Hibernate",
							       "Whether the system can hibernate",
							       FALSE,
							       G_PARAM_READABLE));

	g_object_class_install_property (object_class,
					 PROP_ON_BATTERY,
					 g_param_spec_boolean ("on-battery",
							       "On Battery",
							       "Whether the system is running on battery",
							       FALSE,
							       G_PARAM_READABLE));

	g_object_class_install_property (object_class,
					 PROP_ON_LOW_BATTERY,
					 g_param_spec_boolean ("on-low-battery",
							       "On Low Battery",
							       "Whether the system is running on battery and if the battery is critically low",
							       FALSE,
							       G_PARAM_READABLE));

	g_object_class_install_property (object_class,
					 PROP_LID_IS_CLOSED,
					 g_param_spec_boolean ("lid-is-closed",
							       "Laptop lid is closed",
							       "If the laptop lid is closed",
							       FALSE,
							       G_PARAM_READABLE));

	dbus_g_object_type_install_info (DKP_TYPE_DAEMON, &dbus_glib_dkp_daemon_object_info);

	dbus_g_error_domain_register (DKP_DAEMON_ERROR, NULL, DKP_DAEMON_TYPE_ERROR);
}

/**
 * dkp_daemon_check_state:
 **/
static gboolean
dkp_daemon_check_state (DkpDaemon *daemon)
{
	gchar *contents = NULL;
	GError *error = NULL;
	gboolean ret;
	const gchar *filename = "/sys/power/state";

	/* see what kernel can do */
	ret = g_file_get_contents (filename, &contents, NULL, &error);
	if (!ret) {
		egg_warning ("failed to open %s: %s", filename, error->message);
		g_error_free (error);
		goto out;
	}

	/* does the kernel advertise this */
	daemon->priv->can_suspend = (g_strstr_len (contents, -1, "mem") != NULL);
	daemon->priv->can_hibernate = (g_strstr_len (contents, -1, "disk") != NULL);
out:
	g_free (contents);
	return ret;
}

/**
 * dkp_daemon_check_swap:
 **/
static gfloat
dkp_daemon_check_swap (DkpDaemon *daemon)
{
	gchar *contents = NULL;
	gchar **lines = NULL;
	GError *error = NULL;
	gchar **tokens;
	gboolean ret;
	guint active = 0;
	guint swap_free = 0;
	guint len;
	guint i;
	gfloat percentage = 0.0f;
	const gchar *filename = "/proc/meminfo";

	/* get memory data */
	ret = g_file_get_contents (filename, &contents, NULL, &error);
	if (!ret) {
		egg_warning ("failed to open %s: %s", filename, error->message);
		g_error_free (error);
		goto out;
	}

	/* process each line */
	lines = g_strsplit (contents, "\n", -1);
	for (i=1; lines[i] != NULL; i++) {
		tokens = g_strsplit_set (lines[i], ": ", -1);
		len = g_strv_length (tokens);
		if (len > 3) {
			if (g_strcmp0 (tokens[0], "SwapFree") == 0)
				swap_free = atoi (tokens[len-2]);
			else if (g_strcmp0 (tokens[0], "Active") == 0)
				active = atoi (tokens[len-2]);
		}
		g_strfreev (tokens);
	}

	/* work out how close to the line we are */
	if (swap_free > 0 && active > 0)
		percentage = (active * 100) / swap_free;
	egg_debug ("total swap available %i kb, active memory %i kb (%.1f%%)", swap_free, active, percentage);
out:
	g_free (contents);
	g_strfreev (lines);
	return percentage;
}

/**
 * dkp_daemon_init:
 **/
static void
dkp_daemon_init (DkpDaemon *daemon)
{
	gfloat waterline;

	daemon->priv = DKP_DAEMON_GET_PRIVATE (daemon);
	daemon->priv->polkit = dkp_polkit_new ();
	daemon->priv->lid_is_present = FALSE;
	daemon->priv->lid_is_closed = FALSE;
	daemon->priv->can_suspend = FALSE;
	daemon->priv->can_hibernate = FALSE;

	/* check if we have support */
	dkp_daemon_check_state (daemon);

	/* do we have enough swap? */
	if (daemon->priv->can_hibernate) {
		waterline = dkp_daemon_check_swap (daemon);
		if (waterline > DKP_DAEMON_SWAP_WATERLINE) {
			egg_debug ("not enough swap to enable hibernate");
			daemon->priv->can_hibernate = FALSE;
		}
	}
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

	if (daemon->priv->proxy != NULL)
		g_object_unref (daemon->priv->proxy);
	if (daemon->priv->connection != NULL)
		dbus_g_connection_unref (daemon->priv->connection);
	if (daemon->priv->gudev_client != NULL)
		g_object_unref (daemon->priv->gudev_client);
	if (daemon->priv->power_devices != NULL)
		g_object_unref (daemon->priv->power_devices);
	if (daemon->priv->managed_devices != NULL)
		g_object_unref (daemon->priv->managed_devices);
	g_object_unref (daemon->priv->polkit);

	G_OBJECT_CLASS (dkp_daemon_parent_class)->finalize (object);
}

static gboolean gpk_daemon_device_add (DkpDaemon *daemon, GUdevDevice *d, gboolean emit_event);
static void gpk_daemon_device_remove (DkpDaemon *daemon, GUdevDevice *d);

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
	array = dkp_device_list_get_array (daemon->priv->power_devices);
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
 * dkp_daemon_get_number_devices_of_type:
 **/
guint
dkp_daemon_get_number_devices_of_type (DkpDaemon *daemon, DkpDeviceType type)
{
	guint i;
	DkpDevice *device;
	const GPtrArray *array;
	DkpDeviceType type_tmp;
	guint count = 0;

	/* ask each device */
	array = dkp_device_list_get_array (daemon->priv->power_devices);
	for (i=0; i<array->len; i++) {
		device = (DkpDevice *) g_ptr_array_index (array, i);
		g_object_get (device,
			      "type", &type_tmp,
			      NULL);
		if (type == type_tmp)
			count++;
	}
	return count;
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
	array = dkp_device_list_get_array (daemon->priv->power_devices);
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
 * dkp_daemon_get_on_ac_local:
 *
 * As soon as _any_ ac supply goes online, this is true
 **/
static gboolean
dkp_daemon_get_on_ac_local (DkpDaemon *daemon)
{
	guint i;
	gboolean ret;
	gboolean result = FALSE;
	gboolean online;
	DkpDevice *device;
	const GPtrArray *array;

	/* ask each device */
	array = dkp_device_list_get_array (daemon->priv->power_devices);
	for (i=0; i<array->len; i++) {
		device = (DkpDevice *) g_ptr_array_index (array, i);
		ret = dkp_device_get_online (device, &online);
		if (ret && online) {
			result = TRUE;
			break;
		}
	}
	return result;
}

/**
 * dkp_daemon_set_pmutils_powersave:
 *
 * Uses pm-utils to run scripts in power.d
 **/
static gboolean
dkp_daemon_set_pmutils_powersave (DkpDaemon *daemon, gboolean powersave)
{
	gboolean ret;
	gchar *command;
	GError *error = NULL;

	/* run script from pm-utils */
	command = g_strdup_printf ("/usr/sbin/pm-powersave %s", powersave ? "true" : "false");
	egg_debug ("excuting command: %s", command);
	ret = g_spawn_command_line_async (command, &error);
	if (!ret) {
		egg_warning ("failed to run script: %s", error->message);
		g_error_free (error);
		goto out;
	}
out:
	g_free (command);
	return ret;
}

/**
 * dkp_daemon_refresh_battery_devices:
 **/
static gboolean
dkp_daemon_refresh_battery_devices (DkpDaemon *daemon)
{
	guint i;
	const GPtrArray *array;
	DkpDevice *device;
	DkpDeviceType type;

	/* refresh all devices in array */
	array = dkp_device_list_get_array (daemon->priv->power_devices);
	for (i=0; i<array->len; i++) {
		device = (DkpDevice *) g_ptr_array_index (array, i);
		/* only refresh battery devices */
		g_object_get (device,
			      "type", &type,
			      NULL);
		if (type == DKP_DEVICE_TYPE_BATTERY)
			dkp_device_refresh_internal (device);
	}

	return TRUE;
}

/**
 * dkp_daemon_refresh_battery_devices_cb:
 **/
static gboolean
dkp_daemon_refresh_battery_devices_cb (DkpDaemon *daemon)
{
	egg_debug ("doing the delayed refresh");
	dkp_daemon_refresh_battery_devices (daemon);
	return FALSE;
}

/**
 * dkp_daemon_device_changed:
 **/
static void
gpk_daemon_device_changed (DkpDaemon *daemon, GUdevDevice *d, gboolean synthesized)
{
	GObject *object;
	DkpDevice *device;
	gboolean ret;

	/* first, change the device and add it if it doesn't exist */
	object = dkp_device_list_lookup (daemon->priv->power_devices, d);
	if (object != NULL) {
		device = DKP_DEVICE (object);
		egg_debug ("changed %s", dkp_device_get_object_path (device));
		dkp_device_changed (device, d, synthesized);
	} else {
		egg_debug ("treating change event as add on %s", g_udev_device_get_sysfs_path (d));
		dkp_daemon_device_add (daemon, d, TRUE);
	}

	/* second, check if the on_battery and low_battery state has changed */
	ret = (dkp_daemon_get_on_battery_local (daemon) && !dkp_daemon_get_on_ac_local (daemon));
	if (ret != daemon->priv->on_battery) {
		daemon->priv->on_battery = ret;
		egg_debug ("now on_battery = %s", ret ? "yes" : "no");
		g_signal_emit (daemon, signals[CHANGED_SIGNAL], 0);

		/* refresh all the devices now */
		dkp_daemon_refresh_battery_devices (daemon);

		/* refresh again in a little while */
		g_timeout_add_seconds (DKP_DAEMON_ON_BATTERY_REFRESH_DEVICES_DELAY,
				       (GSourceFunc) dkp_daemon_refresh_battery_devices_cb, daemon);

		/* set pm-utils power policy */
		dkp_daemon_set_pmutils_powersave (daemon, daemon->priv->on_battery);
	}
	ret = dkp_daemon_get_low_battery_local (daemon);
	if (ret != daemon->priv->low_battery) {
		daemon->priv->low_battery = ret;
		egg_debug ("now low_battery = %s", ret ? "yes" : "no");
		g_signal_emit (daemon, signals[CHANGED_SIGNAL], 0);
	}
}

/**
 * dkp_daemon_device_went_away:
 **/
static void
dkp_daemon_device_went_away (gpointer user_data, GObject *device)
{
	DkpDaemon *daemon = DKP_DAEMON (user_data);
	dkp_device_list_remove (daemon->priv->power_devices, device);
}

/**
 * dkp_daemon_device_get:
 **/
static DkpDevice *
gpk_daemon_device_get (DkpDaemon *daemon, GUdevDevice *d)
{
	const gchar *subsys;
	const gchar *native_path;
	DkpDevice *device = NULL;
	DkpInput *input;
	gboolean ret;

	subsys = g_udev_device_get_subsystem (d);
	if (g_strcmp0 (subsys, "power_supply") == 0) {

		/* are we a valid power supply */
		device = DKP_DEVICE (dkp_device_supply_new ());
		ret = dkp_device_coldplug (device, daemon, d);
		if (ret)
			goto out;
		g_object_unref (device);

		/* no valid power supply object */
		device = NULL;

	} else if (g_strcmp0 (subsys, "tty") == 0) {

		/* try to detect a Watts Up? Pro monitor */
		device = DKP_DEVICE (dkp_device_wup_new ());
		ret = dkp_device_coldplug (device, daemon, d);
		if (ret)
			goto out;
		g_object_unref (device);

		/* no valid TTY object ;-( */
		device = NULL;

	} else if (g_strcmp0 (subsys, "usb") == 0) {

		/* see if this is a CSR mouse or keyboard */
		device = DKP_DEVICE (dkp_device_csr_new ());
		ret = dkp_device_coldplug (device, daemon, d);
		if (ret)
			goto out;
		g_object_unref (device);

		/* try to detect a HID UPS */
		device = DKP_DEVICE (dkp_device_hid_new ());
		ret = dkp_device_coldplug (device, daemon, d);
		if (ret)
			goto out;
		g_object_unref (device);

		/* no valid USB object ;-( */
		device = NULL;

	} else if (g_strcmp0 (subsys, "input") == 0) {

		/* check input device */
		input = dkp_input_new ();
		ret = dkp_input_coldplug (input, daemon, d);
		if (!ret) {
			g_object_unref (input);
			goto out;
		}

		/* we now have a lid */
		daemon->priv->lid_is_present = TRUE;

		/* not a power device */
		dkp_device_list_insert (daemon->priv->managed_devices, d, G_OBJECT (input));

		/* no valid input object */
		device = NULL;

	} else {
		native_path = g_udev_device_get_sysfs_path (d);
		egg_warning ("native path %s (%s) ignoring", native_path, subsys);
	}
out:
	return device;
}

/**
 * dkp_daemon_device_add:
 **/
static gboolean
gpk_daemon_device_add (DkpDaemon *daemon, GUdevDevice *d, gboolean emit_event)
{
	GObject *object;
	DkpDevice *device;
	gboolean ret = TRUE;

	/* does device exist in db? */
	object = dkp_device_list_lookup (daemon->priv->power_devices, d);
	if (object != NULL) {
		device = DKP_DEVICE (object);
		/* we already have the device; treat as change event */
		egg_debug ("treating add event as change event on %s", dkp_device_get_object_path (device));
		dkp_daemon_device_changed (daemon, d, FALSE);
	} else {

		/* get the right sort of device */
		device = dkp_daemon_device_get (daemon, d);
		if (device == NULL) {
			egg_debug ("not adding device %s", g_udev_device_get_sysfs_path (d));
			ret = FALSE;
			goto out;
		}
		/* only take a weak ref; the device will stay on the bus until
		 * it's unreffed. So if we ref it, it'll never go away.
		 */
		g_object_weak_ref (G_OBJECT (device), dkp_daemon_device_went_away, daemon);
		dkp_device_list_insert (daemon->priv->power_devices, d, G_OBJECT (device));
		if (emit_event) {
			g_signal_emit (daemon, signals[DEVICE_ADDED_SIGNAL], 0,
				       dkp_device_get_object_path (device));
		}
	}
out:
	return ret;
}

/**
 * dkp_daemon_device_remove:
 **/
static void
gpk_daemon_device_remove (DkpDaemon *daemon, GUdevDevice *d)
{
	GObject *object;
	DkpDevice *device;

	/* does device exist in db? */
	object = dkp_device_list_lookup (daemon->priv->power_devices, d);
	if (object == NULL) {
		egg_debug ("ignoring remove event on %s", g_udev_device_get_sysfs_path (d));
	} else {
		device = DKP_DEVICE (object);
		dkp_device_removed (device);
		g_signal_emit (daemon, signals[DEVICE_REMOVED_SIGNAL], 0,
			       dkp_device_get_object_path (device));
		g_object_unref (device);
	}
}

/**
 * gpk_daemon_uevent_signal_handler_cb:
 **/
static void
gpk_daemon_uevent_signal_handler_cb (GUdevClient *client, const gchar *action,
				     GUdevDevice *device, gpointer user_data)
{
	DkpDaemon *daemon = DKP_DAEMON (user_data);

	if (g_strcmp0 (action, "add") == 0) {
		egg_debug ("add %s", g_udev_device_get_sysfs_path (device));
		gpk_daemon_device_add (daemon, device, TRUE);
	} else if (g_strcmp0 (action, "remove") == 0) {
		egg_debug ("remove %s", g_udev_device_get_sysfs_path (device));
		gpk_daemon_device_remove (daemon, device);
	} else if (g_strcmp0 (action, "change") == 0) {
		egg_debug ("change %s", g_udev_device_get_sysfs_path (device));
		gpk_daemon_device_changed (daemon, device, FALSE);
	} else {
		egg_warning ("unhandled action '%s' on %s", action, g_udev_device_get_sysfs_path (device));
	}
}

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
	array = dkp_device_list_get_array (daemon->priv->power_devices);
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
 * dkp_daemon_suspend:
 **/
gboolean
dkp_daemon_suspend (DkpDaemon *daemon, DBusGMethodInvocation *context)
{
	gboolean ret;
	GError *error;
	GError *error_local = NULL;
	PolkitSubject *subject;
	gchar *stdout = NULL;
	gchar *stderr = NULL;

	subject = dkp_polkit_get_subject (daemon->priv->polkit, context);
	if (subject == NULL)
		goto out;

	if (!dkp_polkit_check_auth (daemon->priv->polkit, subject, "org.freedesktop.devicekit.power.suspend", context))
		goto out;

	ret = g_spawn_command_line_sync ("/usr/sbin/pm-suspend", &stdout, &stderr, NULL, &error_local);
	if (!ret) {
		error = g_error_new (DKP_DAEMON_ERROR,
				     DKP_DAEMON_ERROR_GENERAL,
				     "Failed to spawn: %s, stdout:%s, stderr:%s", error_local->message, stdout, stderr);
		g_error_free (error_local);
		dbus_g_method_return_error (context, error);
		goto out;
	}
	dbus_g_method_return (context, NULL);
out:
	g_free (stdout);
	g_free (stderr);
	if (subject != NULL)
		g_object_unref (subject);
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
	PolkitSubject *subject;
	gchar *stdout = NULL;
	gchar *stderr = NULL;

	subject = dkp_polkit_get_subject (daemon->priv->polkit, context);
	if (subject == NULL)
		goto out;

	if (!dkp_polkit_check_auth (daemon->priv->polkit, subject, "org.freedesktop.devicekit.power.hibernate", context))
		goto out;

	ret = g_spawn_command_line_sync ("/usr/sbin/pm-hibernate", &stdout, &stderr, NULL, &error_local);
	if (!ret) {
		error = g_error_new (DKP_DAEMON_ERROR,
				     DKP_DAEMON_ERROR_GENERAL,
				     "Failed to spawn: %s, stdout:%s, stderr:%s", error_local->message, stdout, stderr);
		g_error_free (error_local);
		dbus_g_method_return_error (context, error);
		goto out;
	}
	dbus_g_method_return (context, NULL);
out:
	g_free (stdout);
	g_free (stderr);
	if (subject != NULL)
		g_object_unref (subject);
	return TRUE;
}

/**
 * dkp_daemon_register_power_daemon:
 **/
static gboolean
dkp_daemon_register_power_daemon (DkpDaemon *daemon)
{
	DBusConnection *connection;
	DBusError dbus_error;
	GError *error = NULL;
	guint i;

	error = NULL;
	daemon->priv->connection = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
	if (daemon->priv->connection == NULL) {
		if (error != NULL) {
			g_critical ("error getting system bus: %s", error->message);
			g_error_free (error);
		}
		goto error;
	}
	connection = dbus_g_connection_get_connection (daemon->priv->connection);

	dbus_g_connection_register_g_object (daemon->priv->connection,
					     "/org/freedesktop/DeviceKit/Power",
					     G_OBJECT (daemon));

	daemon->priv->proxy = dbus_g_proxy_new_for_name (daemon->priv->connection,
						      DBUS_SERVICE_DBUS,
						      DBUS_PATH_DBUS,
						      DBUS_INTERFACE_DBUS);
	dbus_error_init (&dbus_error);
	if (dbus_error_is_set (&dbus_error)) {
		egg_warning ("Cannot add match rule: %s: %s", dbus_error.name, dbus_error.message);
		dbus_error_free (&dbus_error);
		goto error;
	}

	/* connect to the DeviceKit daemon */
	for (i=0; subsystems[i] != NULL; i++)
		egg_debug ("registering subsystem : %s", subsystems[i]);

	daemon->priv->gudev_client = g_udev_client_new (subsystems);
	g_signal_connect (daemon->priv->gudev_client, "uevent",
			  G_CALLBACK (gpk_daemon_uevent_signal_handler_cb), daemon);

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
	GUdevDevice *device;
	GList *devices;
	GList *l;
	guint i;

	daemon = DKP_DAEMON (g_object_new (DKP_TYPE_DAEMON, NULL));

	daemon->priv->power_devices = dkp_device_list_new ();
	daemon->priv->managed_devices = dkp_device_list_new ();

	if (!dkp_daemon_register_power_daemon (DKP_DAEMON (daemon))) {
		g_object_unref (daemon);
		return NULL;
	}

	/* add all subsystems */
	for (i=0; subsystems[i] != NULL; i++) {
		devices = g_udev_client_query_by_subsystem (daemon->priv->gudev_client, subsystems[i]);
		for (l = devices; l != NULL; l = l->next) {
			device = l->data;
			gpk_daemon_device_add (daemon, device, FALSE);
		}
		g_list_foreach (devices, (GFunc) g_object_unref, NULL);
		g_list_free (devices);
	}

	daemon->priv->on_battery = (dkp_daemon_get_on_battery_local (daemon) &&
				    !dkp_daemon_get_on_ac_local (daemon));
	daemon->priv->low_battery = dkp_daemon_get_low_battery_local (daemon);

	/* set pm-utils power policy */
	dkp_daemon_set_pmutils_powersave (daemon, daemon->priv->on_battery);

	return daemon;
}

