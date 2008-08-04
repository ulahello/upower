/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 David Zeuthen <david@fubar.dk>
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
#include <math.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n-lib.h>
#include <glib-object.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <devkit-gobject.h>
#include <polkit-dbus/polkit-dbus.h>

#include "sysfs-utils.h"
#include "dkp-debug.h"
#include "dkp-enum.h"
#include "dkp-object.h"
#include "dkp-source.h"
#include "dkp-history.h"
#include "dkp-marshal.h"
#include "dkp-source-glue.h"

#define DK_POWER_MIN_CHARGED_PERCENTAGE	60

struct DkpSourcePrivate
{
	DBusGConnection		*system_bus_connection;
	DBusGProxy		*system_bus_proxy;
	DkpDaemon		*daemon;
	DevkitDevice		*d;
	DkpHistory		*history;
	gchar			*object_path;
	guint			 poll_timer_id;
	DkpObject		*obj;
	gboolean		 has_coldplug_values;
	gdouble			 battery_energy_old;
	GTimeVal		 battery_energy_old_timespec;
};

static void	dkp_source_class_init	(DkpSourceClass	*klass);
static void	dkp_source_init		(DkpSource	*source);
static void	dkp_source_finalize	(GObject	*object);
static void	dkp_source_reset_values	(DkpSource	*source);
static gboolean dkp_source_update	(DkpSource	*source);

enum
{
	PROP_0,
	PROP_NATIVE_PATH,
	PROP_VENDOR,
	PROP_MODEL,
	PROP_SERIAL,
	PROP_UPDATE_TIME,
	PROP_TYPE,
	PROP_LINE_POWER_ONLINE,
	PROP_POWER_SUPPLY,
	PROP_BATTERY_CAPACITY,
	PROP_BATTERY_IS_PRESENT,
	PROP_BATTERY_IS_RECHARGEABLE,
	PROP_BATTERY_STATE,
	PROP_BATTERY_ENERGY,
	PROP_BATTERY_ENERGY_EMPTY,
	PROP_BATTERY_ENERGY_FULL,
	PROP_BATTERY_ENERGY_FULL_DESIGN,
	PROP_BATTERY_ENERGY_RATE,
	PROP_BATTERY_TIME_TO_EMPTY,
	PROP_BATTERY_TIME_TO_FULL,
	PROP_BATTERY_PERCENTAGE,
	PROP_BATTERY_TECHNOLOGY,
};

enum
{
	CHANGED_SIGNAL,
	LAST_SIGNAL,
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (DkpSource, dkp_source, DKP_SOURCE_TYPE_DEVICE)
#define DKP_SOURCE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), DKP_SOURCE_TYPE_SOURCE, DkpSourcePrivate))

static const char *dkp_source_get_object_path (DkpDevice *device);
static void	dkp_source_removed	 (DkpDevice *device);
static gboolean	dkp_source_changed	 (DkpDevice *device, DevkitDevice *d, gboolean synthesized);

/**
 * dkp_source_get_property:
 **/
static void
dkp_source_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	DkpSource *source = DKP_SOURCE (object);
	DkpObject *obj = source->priv->obj;

	switch (prop_id) {
	case PROP_NATIVE_PATH:
		g_value_set_string (value, obj->native_path);
		break;
	case PROP_VENDOR:
		g_value_set_string (value, obj->vendor);
		break;
	case PROP_MODEL:
		g_value_set_string (value, obj->model);
		break;
	case PROP_SERIAL:
		g_value_set_string (value, obj->serial);
		break;
	case PROP_UPDATE_TIME:
		g_value_set_uint64 (value, obj->update_time);
		break;
	case PROP_TYPE:
		g_value_set_string (value, dkp_source_type_to_text (obj->type));
		break;
	case PROP_POWER_SUPPLY:
		g_value_set_boolean (value, obj->power_supply);
		break;
	case PROP_LINE_POWER_ONLINE:
		g_value_set_boolean (value, obj->line_power_online);
		break;
	case PROP_BATTERY_IS_PRESENT:
		g_value_set_boolean (value, obj->battery_is_present);
		break;
	case PROP_BATTERY_IS_RECHARGEABLE:
		g_value_set_boolean (value, obj->battery_is_rechargeable);
		break;
	case PROP_BATTERY_STATE:
		g_value_set_string (value, dkp_source_state_to_text (obj->battery_state));
		break;
	case PROP_BATTERY_CAPACITY:
		g_value_set_double (value, obj->battery_capacity);
		break;
	case PROP_BATTERY_ENERGY:
		g_value_set_double (value, obj->battery_energy);
		break;
	case PROP_BATTERY_ENERGY_EMPTY:
		g_value_set_double (value, obj->battery_energy_empty);
		break;
	case PROP_BATTERY_ENERGY_FULL:
		g_value_set_double (value, obj->battery_energy_full);
		break;
	case PROP_BATTERY_ENERGY_FULL_DESIGN:
		g_value_set_double (value, obj->battery_energy_full_design);
		break;
	case PROP_BATTERY_ENERGY_RATE:
		g_value_set_double (value, obj->battery_energy_rate);
		break;
	case PROP_BATTERY_TIME_TO_EMPTY:
		g_value_set_int64 (value, obj->battery_time_to_empty);
		break;
	case PROP_BATTERY_TIME_TO_FULL:
		g_value_set_int64 (value, obj->battery_time_to_full);
		break;
	case PROP_BATTERY_PERCENTAGE:
		g_value_set_double (value, obj->battery_percentage);
		break;
	case PROP_BATTERY_TECHNOLOGY:
		g_value_set_string (value, dkp_source_technology_to_text (obj->battery_technology));
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * dkp_source_class_init:
 **/
static void
dkp_source_class_init (DkpSourceClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	DkpDeviceClass *device_class = DKP_DEVICE_CLASS (klass);

	object_class->finalize = dkp_source_finalize;
	object_class->get_property = dkp_source_get_property;
	device_class->changed = dkp_source_changed;
	device_class->removed = dkp_source_removed;
	device_class->get_object_path = dkp_source_get_object_path;

	g_type_class_add_private (klass, sizeof (DkpSourcePrivate));

	signals[CHANGED_SIGNAL] =
		g_signal_new ("changed",
			      G_OBJECT_CLASS_TYPE (klass),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
			      0, NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	dbus_g_object_type_install_info (DKP_SOURCE_TYPE_SOURCE, &dbus_glib_dkp_source_object_info);

	g_object_class_install_property (
		object_class,
		PROP_NATIVE_PATH,
		g_param_spec_string ("native-path", NULL, NULL, NULL, G_PARAM_READABLE));
	g_object_class_install_property (
		object_class,
		PROP_VENDOR,
		g_param_spec_string ("vendor", NULL, NULL, NULL, G_PARAM_READABLE));
	g_object_class_install_property (
		object_class,
		PROP_MODEL,
		g_param_spec_string ("model", NULL, NULL, NULL, G_PARAM_READABLE));
	g_object_class_install_property (
		object_class,
		PROP_SERIAL,
		g_param_spec_string ("serial", NULL, NULL, NULL, G_PARAM_READABLE));
	g_object_class_install_property (
		object_class,
		PROP_UPDATE_TIME,
		g_param_spec_uint64 ("update-time", NULL, NULL, 0, G_MAXUINT64, 0, G_PARAM_READABLE));
	g_object_class_install_property (
		object_class,
		PROP_TYPE,
		g_param_spec_string ("type", NULL, NULL, NULL, G_PARAM_READABLE));
	g_object_class_install_property (
		object_class,
		PROP_BATTERY_IS_PRESENT,
		g_param_spec_boolean ("power-supply", NULL, NULL, FALSE, G_PARAM_READABLE));
	g_object_class_install_property (
		object_class,
		PROP_LINE_POWER_ONLINE,
		g_param_spec_boolean ("line-power-online", NULL, NULL, FALSE, G_PARAM_READABLE));
	g_object_class_install_property (
		object_class,
		PROP_BATTERY_ENERGY,
		g_param_spec_double ("battery-energy", NULL, NULL, 0, G_MAXDOUBLE, 0, G_PARAM_READABLE));
	g_object_class_install_property (
		object_class,
		PROP_BATTERY_IS_PRESENT,
		g_param_spec_boolean ("battery-is-present", NULL, NULL, FALSE, G_PARAM_READABLE));
	g_object_class_install_property (
		object_class,
		PROP_BATTERY_IS_RECHARGEABLE,
		g_param_spec_boolean ("battery-is-rechargeable", NULL, NULL, FALSE, G_PARAM_READABLE));
	g_object_class_install_property (
		object_class,
		PROP_BATTERY_STATE,
		g_param_spec_string ("battery-state", NULL, NULL, NULL, G_PARAM_READABLE));
	g_object_class_install_property (
		object_class,
		PROP_BATTERY_CAPACITY,
		g_param_spec_double ("battery-capacity", NULL, NULL, 0, G_MAXDOUBLE, 0, G_PARAM_READABLE));
	g_object_class_install_property (
		object_class,
		PROP_BATTERY_ENERGY_EMPTY,
		g_param_spec_double ("battery-energy-empty", NULL, NULL, 0, G_MAXDOUBLE, 0, G_PARAM_READABLE));
	g_object_class_install_property (
		object_class,
		PROP_BATTERY_ENERGY_FULL,
		g_param_spec_double ("battery-energy-full", NULL, NULL, 0, G_MAXDOUBLE, 0, G_PARAM_READABLE));
	g_object_class_install_property (
		object_class,
		PROP_BATTERY_ENERGY_FULL_DESIGN,
		g_param_spec_double ("battery-energy-full-design", NULL, NULL, 0, G_MAXDOUBLE, 0, G_PARAM_READABLE));
	g_object_class_install_property (
		object_class,
		PROP_BATTERY_ENERGY_RATE,
		g_param_spec_double ("battery-energy-rate", NULL, NULL, -G_MAXDOUBLE, G_MAXDOUBLE, 0, G_PARAM_READABLE));
	g_object_class_install_property (
		object_class,
		PROP_BATTERY_TIME_TO_EMPTY,
		g_param_spec_int64 ("battery-time-to-empty", NULL, NULL, -1, G_MAXINT64, -1, G_PARAM_READABLE));
	g_object_class_install_property (
		object_class,
		PROP_BATTERY_TIME_TO_FULL,
		g_param_spec_int64 ("battery-time-to-full", NULL, NULL, -1, G_MAXINT64, -1, G_PARAM_READABLE));
	g_object_class_install_property (
		object_class,
		PROP_BATTERY_PERCENTAGE,
		g_param_spec_double ("battery-percentage", NULL, NULL, -1, 100, -1, G_PARAM_READABLE));
	g_object_class_install_property (
		object_class,
		PROP_BATTERY_TECHNOLOGY,
		g_param_spec_string ("battery-technology", NULL, NULL, NULL, G_PARAM_READABLE));
}

/**
 * dkp_source_init:
 **/
static void
dkp_source_init (DkpSource *source)
{
	source->priv = DKP_SOURCE_GET_PRIVATE (source);
	dkp_source_reset_values (source);
}

/**
 * dkp_source_finalize:
 **/
static void
dkp_source_finalize (GObject *object)
{
	DkpSource *source;

	g_return_if_fail (object != NULL);
	g_return_if_fail (DKP_IS_SOURCE (object));

	source = DKP_SOURCE (object);
	g_return_if_fail (source->priv != NULL);

	g_object_unref (source->priv->d);
	g_object_unref (source->priv->daemon);
	g_object_unref (source->priv->history);
	dkp_object_free (source->priv->obj);

	if (source->priv->poll_timer_id > 0)
		g_source_remove (source->priv->poll_timer_id);

	G_OBJECT_CLASS (dkp_source_parent_class)->finalize (object);
}

/**
 * dkp_source_compute_object_path_from_basename:
 **/
static char *
dkp_source_compute_object_path_from_basename (const char *native_path_basename)
{
	gchar *basename;
	gchar *object_path;
	unsigned int n;

	/* TODO: need to be more thorough with making proper object
	 * names that won't make D-Bus crash. This is just to cope
	 * with dm-0...
	 */
	basename = g_path_get_basename (native_path_basename);
	for (n = 0; basename[n] != '\0'; n++)
		if (basename[n] == '-')
			basename[n] = '_';
	object_path = g_build_filename ("/sources/", basename, NULL);
	g_free (basename);

	return object_path;
}

/**
 * dkp_source_compute_object_path:
 **/
static gchar *
dkp_source_compute_object_path (const char *native_path)
{
	gchar *basename;
	gchar *object_path;

	basename = g_path_get_basename (native_path);
	object_path = dkp_source_compute_object_path_from_basename (basename);
	g_free (basename);
	return object_path;
}

/**
 * dkp_source_register_power_source:
 **/
static gboolean
dkp_source_register_power_source (DkpSource *source)
{
	DBusConnection *connection;
	GError *error = NULL;

	source->priv->system_bus_connection = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
	if (source->priv->system_bus_connection == NULL) {
		if (error != NULL) {
			g_critical ("error getting system bus: %s", error->message);
			g_error_free (error);
		}
		goto error;
	}
	connection = dbus_g_connection_get_connection (source->priv->system_bus_connection);

	source->priv->object_path = dkp_source_compute_object_path (source->priv->obj->native_path);

	dbus_g_connection_register_g_object (source->priv->system_bus_connection,
					     source->priv->object_path, G_OBJECT (source));

	source->priv->system_bus_proxy = dbus_g_proxy_new_for_name (source->priv->system_bus_connection,
								    DBUS_SERVICE_DBUS, DBUS_PATH_DBUS, DBUS_INTERFACE_DBUS);

	return TRUE;

error:
	return FALSE;
}

/**
 * dkp_source_new:
 **/
DkpSource *
dkp_source_new (DkpDaemon *daemon, DevkitDevice *d)
{
	DkpSource *source;
	const gchar *native_path;
	gchar *id;

	source = NULL;
	native_path = devkit_device_get_native_path (d);

	source = DKP_SOURCE (g_object_new (DKP_SOURCE_TYPE_SOURCE, NULL));
	source->priv->d = g_object_ref (d);
	source->priv->daemon = g_object_ref (daemon);
	source->priv->obj = dkp_object_new ();
	source->priv->obj->native_path = g_strdup (native_path);
	source->priv->history = dkp_history_new ();

	if (sysfs_file_exists (native_path, "online")) {
		source->priv->obj->type = DKP_SOURCE_TYPE_LINE_POWER;
	} else {
		/* this is correct, UPS and CSR are not in the kernel */
		source->priv->obj->type = DKP_SOURCE_TYPE_BATTERY;
	}

	if (!dkp_source_update (source)) {
		g_object_unref (source);
		source = NULL;
		goto out;
	}

	if (!dkp_source_register_power_source (DKP_SOURCE (source))) {
		g_object_unref (source);
		source = NULL;
		goto out;
	}

	/* get the id so we can load the old history */
	id = dkp_object_get_id (source->priv->obj);
	if (id == NULL) {
		dkp_debug ("cannot get device ID, not loading history");
		goto out;
	}
	dkp_history_set_id (source->priv->history, id);
	g_free (id);

out:
	return source;
}

/**
 * dkp_source_emit_changed:
 **/
static void
dkp_source_emit_changed (DkpSource *source)
{
	dkp_debug ("emitting changed on %s", source->priv->obj->native_path);
	g_signal_emit_by_name (source->priv->daemon, "device-changed",
			       source->priv->object_path, NULL);
	g_signal_emit (source, signals[CHANGED_SIGNAL], 0);
}

/**
 * dkp_source_changed:
 **/
static gboolean
dkp_source_changed (DkpDevice *device, DevkitDevice *d, gboolean synthesized)
{
	DkpSource *source = DKP_SOURCE (device);
	gboolean keep_source;

	g_object_unref (source->priv->d);
	source->priv->d = g_object_ref (d);

	keep_source = dkp_source_update (source);

	/* this 'change' event might prompt us to remove the source */
	if (!keep_source)
		goto out;

	/* no, it's good .. keep it */
	dkp_source_emit_changed (source);

out:
	return keep_source;
}

/**
 * dkp_source_removed:
 **/
void
dkp_source_removed (DkpDevice *device)
{
}

/**
 * dkp_source_get_object_path:
 **/
static const char *
dkp_source_get_object_path (DkpDevice *device)
{
	DkpSource *source = DKP_SOURCE (device);
	return source->priv->object_path;
}

/**
 * dkp_source_update_line_power:
 **/
static gboolean
dkp_source_update_line_power (DkpSource *source)
{
	DkpObject *obj = source->priv->obj;
	DkpObject *obj_old;
	gboolean ret;

	/* make a copy so we can see if anything changed */
	obj_old = dkp_object_copy (obj);

	/* force true */
	obj->power_supply = TRUE;

	/* get new AC value */
	obj->line_power_online = sysfs_get_int (obj->native_path, "online");

	/* initial value */
	if (!source->priv->has_coldplug_values) {
		dkp_object_print (obj);
		source->priv->has_coldplug_values = TRUE;
		goto out;
	}

	/* print difference */
	ret = !dkp_object_equal (obj, obj_old);
	if (ret)
		dkp_object_diff (obj_old, obj);
out:
	dkp_object_free (obj_old);
	return TRUE;
}

/**
 * dkp_source_reset_values:
 **/
static void
dkp_source_reset_values (DkpSource *source)
{
	source->priv->has_coldplug_values = FALSE;
	source->priv->battery_energy_old = -1;
	source->priv->battery_energy_old_timespec.tv_sec = 0;
	dkp_object_clear (source->priv->obj);
}

/**
 * dkp_source_get_id:
 **/
gchar *
dkp_source_get_id (DkpSource *source)
{
	return dkp_object_get_id (source->priv->obj);
}

/**
 * dkp_source_calculate_battery_rate:
 **/
static void
dkp_source_calculate_battery_rate (DkpSource *source)
{
	guint time;
	gdouble energy;
	GTimeVal now;
	DkpObject *obj = source->priv->obj;

	if (obj->battery_energy < 0)
		return;

	if (source->priv->battery_energy_old < 0)
		return;

	if (source->priv->battery_energy_old == obj->battery_energy)
		return;

	/* get the time difference */
	g_get_current_time (&now);
	time = now.tv_sec - source->priv->battery_energy_old_timespec.tv_sec;

	if (time == 0)
		return;

	/* get the difference in charge */
	energy = source->priv->battery_energy_old - obj->battery_energy;
	if (energy < 0.1)
		return;

	/* probably okay */
	obj->battery_energy_rate = energy * 3600 / time;
}

/**
 * dkp_source_update_battery:
 *
 * Return value: TRUE if we changed
 **/
static gboolean
dkp_source_update_battery (DkpSource *source)
{
	gchar *status = NULL;
	gboolean ret;
	gboolean just_added = FALSE;
	gboolean is_charging;
	gboolean is_discharging;
	DkpSourceState battery_state;
	DkpObject *obj = source->priv->obj;
	DkpObject *obj_old;

	/* make a copy so we can see if anything changed */
	obj_old = dkp_object_copy (obj);

	/* have we just been removed? */
	obj->battery_is_present = sysfs_get_bool (obj->native_path, "present");
	if (!obj->battery_is_present) {
		dkp_source_reset_values (source);
		obj->type = DKP_SOURCE_TYPE_BATTERY;
		goto out;
	}

	/* initial values */
	if (!source->priv->has_coldplug_values) {
		gchar *technology_native;

		/* when we add via sysfs power_supply class then we know this is true */
		obj->power_supply = TRUE;

		/* the ACPI spec is bad at defining battery type constants */
		technology_native = g_strstrip (sysfs_get_string (obj->native_path, "technology"));
		obj->battery_technology = dkp_acpi_to_source_technology (technology_native);
		g_free (technology_native);

		obj->vendor = g_strstrip (sysfs_get_string (obj->native_path, "manufacturer"));
		obj->model = g_strstrip (sysfs_get_string (obj->native_path, "model_name"));
		obj->serial = g_strstrip (sysfs_get_string (obj->native_path, "serial_number"));

		/* assume true for laptops */
		obj->battery_is_rechargeable = TRUE;

		/* these don't change at runtime */
		obj->battery_energy_full =
			sysfs_get_double (obj->native_path, "energy_full") / 1000000.0;
		obj->battery_energy_full_design =
			sysfs_get_double (obj->native_path, "energy_full_design") / 1000000.0;

		/* the last full cannot be bigger than the design */
		if (obj->battery_energy_full > obj->battery_energy_full_design)
			obj->battery_energy_full = obj->battery_energy_full_design;

		/* calculate how broken our battery is */
		obj->battery_capacity = obj->battery_energy_full_design / obj->battery_energy_full * 100.0f;
		if (obj->battery_capacity < 0)
			obj->battery_capacity = 0;
		if (obj->battery_capacity > 100.0)
			obj->battery_capacity = 100.0;

		/* we only coldplug once, as these values will never change */
		source->priv->has_coldplug_values = TRUE;
		just_added = TRUE;
	}

	status = g_strstrip (sysfs_get_string (obj->native_path, "status"));
	is_charging = strcasecmp (status, "charging") == 0;
	is_discharging = strcasecmp (status, "discharging") == 0;

	/* really broken battery, assume charging */
	if (is_charging && is_discharging)
		is_discharging = FALSE;

	/* get the currect charge */
	obj->battery_energy =
		sysfs_get_double (obj->native_path, "energy_avg") / 1000000.0;
	if (obj->battery_energy == 0)
		obj->battery_energy =
			sysfs_get_double (obj->native_path, "energy_now") / 1000000.0;

	/* some batteries don't update last_full attribute */
	if (obj->battery_energy > obj->battery_energy_full)
		obj->battery_energy_full = obj->battery_energy;

	obj->battery_energy_rate =
		fabs (sysfs_get_double (obj->native_path, "current_now") / 1000000.0);

	/* ACPI gives out the special 'Ones' value for rate when it's unable
	 * to calculate the true rate. We should set the rate zero, and wait
	 * for the BIOS to stabilise. */
	if (obj->battery_energy_rate == 0xffff)
		obj->battery_energy_rate = -1;

	/* sanity check to less than 100W */
	if (obj->battery_energy_rate > 100*1000)
		obj->battery_energy_rate = -1;

	/* the hardware reporting failed -- try to calculate this */
	if (obj->battery_energy_rate < 0) {
		dkp_source_calculate_battery_rate (source);
	}

	/* charging has a negative rate */
	if (obj->battery_energy_rate > 0 && is_charging)
		obj->battery_energy_rate *= -1.0;

	/* get a precise percentage */
	obj->battery_percentage = 100.0 * obj->battery_energy / obj->battery_energy_full;
	if (obj->battery_percentage < 0)
		obj->battery_percentage = 0;
	if (obj->battery_percentage > 100.0)
		obj->battery_percentage = 100.0;

	/* calculate a quick and dirty time remaining value */
	obj->battery_time_to_empty = -1;
	obj->battery_time_to_full = -1;
	if (obj->battery_energy_rate > 0) {
		if (is_discharging) {
			obj->battery_time_to_empty = 3600 * (obj->battery_energy / obj->battery_energy_rate);
		} else if (is_charging) {
			obj->battery_time_to_full = 3600 * ((obj->battery_energy_full - obj->battery_energy) / obj->battery_energy_rate);
		}
	}
	/* check the remaining time is under a set limit, to deal with broken
	   primary batteries rate */
	if (obj->battery_time_to_empty > (100 * 60 * 60))
		obj->battery_time_to_empty = -1;
	if (obj->battery_time_to_full > (100 * 60 * 60))
		obj->battery_time_to_full = -1;

	/* get the state */
	if (is_charging)
		battery_state = DKP_SOURCE_STATE_CHARGING;
	else if (is_discharging)
		battery_state = DKP_SOURCE_STATE_DISCHARGING;
	else if (obj->battery_percentage > DK_POWER_MIN_CHARGED_PERCENTAGE)
		battery_state = DKP_SOURCE_STATE_FULLY_CHARGED;
	else
		battery_state = DKP_SOURCE_STATE_EMPTY;

	/* set the old status */
	source->priv->battery_energy_old = obj->battery_energy;
	g_get_current_time (&source->priv->battery_energy_old_timespec);

	/* we changed state */
	if (obj->battery_state != battery_state) {
		source->priv->battery_energy_old = -1;
		obj->battery_state = battery_state;
	}

out:
	/* did anything change? */
	ret = !dkp_object_equal (obj, obj_old);
	if (!just_added && ret)
		dkp_object_diff (obj_old, obj);
	dkp_object_free (obj_old);

	/* just for debugging */
	if (just_added)
		dkp_object_print (obj);

	/* save new history */
	if (ret) {
		dkp_history_set_state (source->priv->history, obj->battery_state);
		dkp_history_set_charge_data (source->priv->history, obj->battery_percentage);
		dkp_history_set_rate_data (source->priv->history, obj->battery_energy_rate);
	}

	g_free (status);
	return ret;
}

/**
 * dkp_source_poll_battery:
 **/
static gboolean
dkp_source_poll_battery (DkpSource *source)
{
	gboolean ret;
	DkpObject *obj = source->priv->obj;

	dkp_debug ("No updates on source %s for 30 seconds; forcing update", obj->native_path);
	source->priv->poll_timer_id = 0;
	ret = dkp_source_update (source);
	if (ret) {
		dkp_source_emit_changed (source);
	}
	return FALSE;
}

/**
 * dkp_source_update:
 *
 * Return value: TRUE if we changed
 **/
static gboolean
dkp_source_update (DkpSource *source)
{
	gboolean ret;
	GTimeVal time;
	DkpObject *obj = source->priv->obj;

	if (source->priv->poll_timer_id > 0) {
		g_source_remove (source->priv->poll_timer_id);
		source->priv->poll_timer_id = 0;
	}

	g_get_current_time (&time);
	obj->update_time = time.tv_sec;

	switch (source->priv->obj->type) {
	case DKP_SOURCE_TYPE_LINE_POWER:
		ret = dkp_source_update_line_power (source);
		break;
	case DKP_SOURCE_TYPE_BATTERY:
		ret = dkp_source_update_battery (source);
		/* Seems that we don't get change uevents from the
		 * kernel on some BIOS types; set up a timer to poll
		 * if we are charging or discharging */
		if (obj->battery_state == DKP_SOURCE_STATE_CHARGING ||
		    obj->battery_state == DKP_SOURCE_STATE_DISCHARGING)
			source->priv->poll_timer_id = g_timeout_add_seconds (30, (GSourceFunc) dkp_source_poll_battery, source);
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	return ret;
}

/**
 * dkp_source_refresh:
 **/
gboolean
dkp_source_refresh (DkpSource *power_source, DBusGMethodInvocation *context)
{
	dkp_source_update (power_source);
	dbus_g_method_return (context);
	return TRUE;
}
