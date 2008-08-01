/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
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
#include "dkp-enum.h"
#include "dkp-source.h"
#include "dkp-marshal.h"

/*--------------------------------------------------------------------------------------------------------------*/
#include "dkp-source-glue.h"

#define DK_POWER_MIN_CHARGED_PERCENTAGE	60

struct DkpSourcePrivate
{
        DBusGConnection *system_bus_connection;
        DBusGProxy      *system_bus_proxy;
        DkpDaemon *daemon;
        DevkitDevice *d;

        char *object_path;
        char *native_path;

        guint poll_timer_id;

        char *vendor;
        char *model;
        char *serial;
        GTimeVal update_time;
        DkpSourceType type;
        gboolean has_coldplug_values;

        gboolean power_supply;
        gboolean line_power_online;
        gboolean battery_is_present;
        gboolean battery_is_rechargeable;
        DkpSourceState battery_state;
        DkpSourceTechnology battery_technology;

        double battery_capacity;
        double battery_energy;
        double battery_energy_empty;
        double battery_energy_full;
        double battery_energy_full_design;
        double battery_energy_rate;
        gint64 battery_time_to_empty;
        gint64 battery_time_to_full;
        double battery_percentage;

        double battery_energy_old;
        GTimeVal battery_energy_old_timespec;
};

static void     dkp_source_class_init  (DkpSourceClass *klass);
static void     dkp_source_init        (DkpSource      *source);
static void     dkp_source_finalize    (GObject                *object);
static void     dkp_source_reset_values(DkpSource      *source);

static gboolean update (DkpSource *source);

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
static void        dkp_source_removed         (DkpDevice *device);
static gboolean    dkp_source_changed         (DkpDevice *device,
                                                         DevkitDevice      *d,
                                                         gboolean           synthesized);

static void
get_property (GObject         *object,
              guint            prop_id,
              GValue          *value,
              GParamSpec      *pspec)
{
        DkpSource *source = DKP_SOURCE (object);

        switch (prop_id) {
        case PROP_NATIVE_PATH:
                g_value_set_string (value, source->priv->native_path);
                break;
        case PROP_VENDOR:
                g_value_set_string (value, source->priv->vendor);
                break;
        case PROP_MODEL:
                g_value_set_string (value, source->priv->model);
                break;
        case PROP_SERIAL:
                g_value_set_string (value, source->priv->serial);
                break;
        case PROP_UPDATE_TIME:
                g_value_set_uint64 (value, source->priv->update_time.tv_sec);
                break;
        case PROP_TYPE:
                g_value_set_string (value, dkp_source_type_to_text (source->priv->type));
                break;

        case PROP_POWER_SUPPLY:
                g_value_set_boolean (value, source->priv->power_supply);
                break;

        case PROP_LINE_POWER_ONLINE:
                g_value_set_boolean (value, source->priv->line_power_online);
                break;

        case PROP_BATTERY_IS_PRESENT:
                g_value_set_boolean (value, source->priv->battery_is_present);
                break;
        case PROP_BATTERY_IS_RECHARGEABLE:
                g_value_set_boolean (value, source->priv->battery_is_rechargeable);
                break;
        case PROP_BATTERY_STATE:
                g_value_set_string (value, dkp_source_state_to_text (source->priv->battery_state));
                break;
        case PROP_BATTERY_CAPACITY:
                g_value_set_double (value, source->priv->battery_capacity);
                break;
        case PROP_BATTERY_ENERGY:
                g_value_set_double (value, source->priv->battery_energy);
                break;
        case PROP_BATTERY_ENERGY_EMPTY:
                g_value_set_double (value, source->priv->battery_energy_empty);
                break;
        case PROP_BATTERY_ENERGY_FULL:
                g_value_set_double (value, source->priv->battery_energy_full);
                break;
        case PROP_BATTERY_ENERGY_FULL_DESIGN:
                g_value_set_double (value, source->priv->battery_energy_full_design);
                break;
        case PROP_BATTERY_ENERGY_RATE:
                g_value_set_double (value, source->priv->battery_energy_rate);
                break;
        case PROP_BATTERY_TIME_TO_EMPTY:
                g_value_set_int64 (value, source->priv->battery_time_to_empty);
                break;
        case PROP_BATTERY_TIME_TO_FULL:
                g_value_set_int64 (value, source->priv->battery_time_to_full);
                break;
        case PROP_BATTERY_PERCENTAGE:
                g_value_set_double (value, source->priv->battery_percentage);
                break;

        case PROP_BATTERY_TECHNOLOGY:
                g_value_set_string (value, dkp_source_technology_to_text (source->priv->battery_technology));
                break;

        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}



static void
dkp_source_class_init (DkpSourceClass *klass)
{
        GObjectClass           *object_class = G_OBJECT_CLASS (klass);
        DkpDeviceClass *device_class = DKP_DEVICE_CLASS (klass);

        object_class->finalize = dkp_source_finalize;
        object_class->get_property = get_property;
        device_class->changed = dkp_source_changed;
        device_class->removed = dkp_source_removed;
        device_class->get_object_path = dkp_source_get_object_path;

        g_type_class_add_private (klass, sizeof (DkpSourcePrivate));

        signals[CHANGED_SIGNAL] =
                g_signal_new ("changed",
                              G_OBJECT_CLASS_TYPE (klass),
                              G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                              0,
                              NULL, NULL,
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

static void
dkp_source_init (DkpSource *source)
{
        source->priv = DKP_SOURCE_GET_PRIVATE (source);
        dkp_source_reset_values (source);
}

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

        g_free (source->priv->native_path);

        g_free (source->priv->vendor);
        g_free (source->priv->model);
        g_free (source->priv->serial);

        if (source->priv->poll_timer_id > 0)
                g_source_remove (source->priv->poll_timer_id);

        G_OBJECT_CLASS (dkp_source_parent_class)->finalize (object);
}

static char *
compute_object_path_from_basename (const char *native_path_basename)
{
        char *basename;
        char *object_path;
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

static char *
compute_object_path (const char *native_path)
{
        char *basename;
        char *object_path;

        basename = g_path_get_basename (native_path);
        object_path = compute_object_path_from_basename (basename);
        g_free (basename);
        return object_path;
}

static gboolean
register_power_source (DkpSource *source)
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

        source->priv->object_path = compute_object_path (source->priv->native_path);

        dbus_g_connection_register_g_object (source->priv->system_bus_connection,
                                             source->priv->object_path,
                                             G_OBJECT (source));

        source->priv->system_bus_proxy = dbus_g_proxy_new_for_name (source->priv->system_bus_connection,
                                                                    DBUS_SERVICE_DBUS,
                                                                    DBUS_PATH_DBUS,
                                                                    DBUS_INTERFACE_DBUS);

        return TRUE;

error:
        return FALSE;
}

DkpSource *
dkp_source_new (DkpDaemon *daemon, DevkitDevice *d)
{
        DkpSource *source;
        const char *native_path;

        source = NULL;
        native_path = devkit_device_get_native_path (d);

        source = DKP_SOURCE (g_object_new (DKP_SOURCE_TYPE_SOURCE, NULL));
        source->priv->d = g_object_ref (d);
        source->priv->daemon = g_object_ref (daemon);
        source->priv->native_path = g_strdup (native_path);

        if (sysfs_file_exists (native_path, "online")) {
                source->priv->type = DKP_SOURCE_TYPE_LINE_POWER;
        } else {
                /* this is correct, UPS and CSR are not in the kernel */
                source->priv->type = DKP_SOURCE_TYPE_BATTERY;
        }

        if (!update (source)) {
                g_object_unref (source);
                source = NULL;
                goto out;
        }

        if (! register_power_source (DKP_SOURCE (source))) {
                g_object_unref (source);
                source = NULL;
                goto out;
        }

out:
        return source;
}

static void
emit_changed (DkpSource *source)
{
        g_print ("emitting changed on %s\n", source->priv->native_path);
        g_signal_emit_by_name (source->priv->daemon,
                               "device-changed",
                               source->priv->object_path,
                               NULL);
        g_signal_emit (source, signals[CHANGED_SIGNAL], 0);
}

static gboolean
dkp_source_changed (DkpDevice *device, DevkitDevice *d, gboolean synthesized)
{
        DkpSource *source = DKP_SOURCE (device);
        gboolean keep_source;

        g_object_unref (source->priv->d);
        source->priv->d = g_object_ref (d);

        keep_source = update (source);

        /* this 'change' event might prompt us to remove the source */
        if (!keep_source)
                goto out;

        /* no, it's good .. keep it */
        emit_changed (source);

out:
        return keep_source;
}

void
dkp_source_removed (DkpDevice *device)
{
}

static const char *
dkp_source_get_object_path (DkpDevice *device)
{
        DkpSource *source = DKP_SOURCE (device);
        return source->priv->object_path;
}

/*--------------------------------------------------------------------------------------------------------------*/

static gboolean
update_line_power (DkpSource *source)
{
        source->priv->line_power_online = sysfs_get_int (source->priv->native_path, "online");
        return TRUE;
}

static void
dkp_source_reset_values (DkpSource *source)
{
        source->priv->battery_energy = -1;
        source->priv->battery_energy_old = -1;
        source->priv->battery_energy_full = -1;
        source->priv->battery_energy_full_design = -1;
        source->priv->battery_energy_rate = -1;
        source->priv->battery_percentage = -1;
        source->priv->battery_capacity = -1;
        source->priv->battery_time_to_empty = -1;
        source->priv->battery_time_to_full = -1;
        source->priv->battery_state = DKP_SOURCE_STATE_UNKNOWN;
        source->priv->battery_technology = DKP_SOURCE_TECHNOLGY_UNKNOWN;
        source->priv->vendor = NULL;
        source->priv->model = NULL;
        source->priv->serial = NULL;
        source->priv->line_power_online = FALSE;
        source->priv->battery_is_present = FALSE;
        source->priv->power_supply = FALSE;
        source->priv->battery_is_rechargeable = FALSE;
        source->priv->has_coldplug_values = FALSE;
        source->priv->battery_energy_old_timespec.tv_sec = 0;
}

gchar *
dkp_source_get_id (DkpSource *source)
{
	GString *string;
	gchar *id = NULL;

        /* only valid for devices supplying the system */
        if (!source->priv->power_supply)
                return id;

        /* only valid for batteries */
        if (source->priv->type != DKP_SOURCE_TYPE_BATTERY)
                return id;

        /* we don't have an ID if we are not present */
        if (!source->priv->battery_is_present)
                return id;

	string = g_string_new ("");

	/* in an ideal world, model-capacity-serial */
	if (source->priv->model != NULL && strlen (source->priv->model) > 2) {
		g_string_append (string, source->priv->model);
		g_string_append_c (string, '-');
	}
	if (source->priv->battery_energy_full_design > 0) {
		g_string_append_printf (string, "%i", (guint) source->priv->battery_energy_full_design);
		g_string_append_c (string, '-');
	}
	if (source->priv->serial != NULL && strlen (source->priv->serial) > 2) {
		g_string_append (string, source->priv->serial);
		g_string_append_c (string, '-');
	}

	/* make sure we are sane */
	if (string->len == 0) {
		/* just use something generic */
		g_string_append (string, "generic_id");
	} else {
		/* remove trailing '-' */
		g_string_set_size (string, string->len - 1);
	}

	/* the id may have invalid chars that need to be replaced */
	id = g_string_free (string, FALSE);
	g_strdelimit (id, "\\\t\"' /", '_');

	return id;
}

static void
calculate_battery_rate (DkpSource *source)
{
        guint time;
        gdouble energy;
        GTimeVal now;

        if (source->priv->battery_energy < 0)
                return;

        if (source->priv->battery_energy_old < 0)
                return;

        if (source->priv->battery_energy_old == source->priv->battery_energy)
                return;

        /* get the time difference */
        g_get_current_time (&now);
        time = now.tv_sec - source->priv->battery_energy_old_timespec.tv_sec;

        if (time == 0)
                return;

        /* get the difference in charge */
        energy = source->priv->battery_energy_old - source->priv->battery_energy;
        if (energy < 0.1)
                return;

        /* probably okay */
        source->priv->battery_energy_rate = energy * 3600 / time;
}

static gboolean
update_battery (DkpSource *source)
{
        char *status;
        gboolean is_charging;
        gboolean is_discharging;
        DkpSourceState battery_state;

        /* are we present? */
        source->priv->battery_is_present = sysfs_get_bool (source->priv->native_path, "present");
        if (!source->priv->battery_is_present) {
                g_free (source->priv->vendor);
                g_free (source->priv->model);
                g_free (source->priv->serial);
                dkp_source_reset_values (source);
                return TRUE;
        }

        /* initial values */
        if (!source->priv->has_coldplug_values) {
                char *technology_native;

                /* when we add via sysfs power_supply class then we know this is true */
                source->priv->power_supply = TRUE;

                /* the ACPI spec is bad at defining battery type constants */
                technology_native = g_strstrip (sysfs_get_string (source->priv->native_path, "technology"));
                source->priv->battery_technology = dkp_acpi_to_source_technology (technology_native);
                g_free (technology_native);

                source->priv->vendor = g_strstrip (sysfs_get_string (source->priv->native_path, "manufacturer"));
                source->priv->model = g_strstrip (sysfs_get_string (source->priv->native_path, "model_name"));
                source->priv->serial = g_strstrip (sysfs_get_string (source->priv->native_path, "serial_number"));

                /* assume true for laptops */
                source->priv->battery_is_rechargeable = TRUE;

                /* these don't change at runtime */
                source->priv->battery_energy_full =
                        sysfs_get_double (source->priv->native_path, "energy_full") / 1000000.0;
                source->priv->battery_energy_full_design =
                        sysfs_get_double (source->priv->native_path, "energy_full_design") / 1000000.0;

                /* the last full cannot be bigger than the design */
                if (source->priv->battery_energy_full > source->priv->battery_energy_full_design)
                        source->priv->battery_energy_full = source->priv->battery_energy_full_design;

                /* calculate how broken our battery is */
                source->priv->battery_capacity = source->priv->battery_energy_full_design /
                                                 source->priv->battery_energy_full * 100.0f;
                if (source->priv->battery_capacity < 0)
                        source->priv->battery_capacity = 0;
                if (source->priv->battery_capacity > 100.0)
                        source->priv->battery_capacity = 100.0;

                /* we only coldplug once, as these values will never change */
                source->priv->has_coldplug_values = TRUE;
        }

        status = g_strstrip (sysfs_get_string (source->priv->native_path, "status"));
        is_charging = strcasecmp (status, "charging") == 0;
        is_discharging = strcasecmp (status, "discharging") == 0;

        /* really broken battery, assume charging */
        if (is_charging && is_discharging)
                is_discharging = FALSE;

        /* get the currect charge */
        source->priv->battery_energy =
                sysfs_get_double (source->priv->native_path, "energy_avg") / 1000000.0;
        if (source->priv->battery_energy == 0)
                source->priv->battery_energy =
                        sysfs_get_double (source->priv->native_path, "energy_now") / 1000000.0;

        /* some batteries don't update last_full attribute */
        if (source->priv->battery_energy > source->priv->battery_energy_full)
                source->priv->battery_energy_full = source->priv->battery_energy;

        source->priv->battery_energy_rate =
                fabs (sysfs_get_double (source->priv->native_path, "current_now") / 1000000.0);

	/* ACPI gives out the special 'Ones' value for rate when it's unable
	 * to calculate the true rate. We should set the rate zero, and wait
	 * for the BIOS to stabilise. */
	if (source->priv->battery_energy_rate == 0xffff)
		source->priv->battery_energy_rate = -1;

	/* sanity check to less than 100W */
	if (source->priv->battery_energy_rate > 100*1000)
		source->priv->battery_energy_rate = -1;

        /* the hardware reporting failed -- try to calculate this */
        if (source->priv->battery_energy_rate < 0) {
                calculate_battery_rate (source);
        }

        /* charging has a negative rate */
        if (source->priv->battery_energy_rate > 0 && is_charging)
                source->priv->battery_energy_rate *= -1.0;

        /* get a precise percentage */
        source->priv->battery_percentage = 100.0 * source->priv->battery_energy / source->priv->battery_energy_full;
        if (source->priv->battery_percentage < 0)
                source->priv->battery_percentage = 0;
        if (source->priv->battery_percentage > 100.0)
                source->priv->battery_percentage = 100.0;

        /* calculate a quick and dirty time remaining value */
        source->priv->battery_time_to_empty = -1;
        source->priv->battery_time_to_full = -1;
	if (source->priv->battery_energy_rate > 0) {
		if (is_discharging) {
			source->priv->battery_time_to_empty = 3600 * (source->priv->battery_energy /
							      source->priv->battery_energy_rate);
		} else if (is_charging) {
			source->priv->battery_time_to_full = 3600 *
				((source->priv->battery_energy_full - source->priv->battery_energy) /
				source->priv->battery_energy_rate);
		}
	}
	/* check the remaining time is under a set limit, to deal with broken
	   primary batteries rate */
	if (source->priv->battery_time_to_empty > (100 * 60 * 60))
		source->priv->battery_time_to_empty = -1;
	if (source->priv->battery_time_to_full > (100 * 60 * 60))
		source->priv->battery_time_to_full = -1;

        /* get the state */
        if (is_charging)
                battery_state = DKP_SOURCE_STATE_CHARGING;
        else if (is_discharging)
                battery_state = DKP_SOURCE_STATE_DISCHARGING;
        else if (source->priv->battery_percentage > DK_POWER_MIN_CHARGED_PERCENTAGE)
                battery_state = DKP_SOURCE_STATE_FULLY_CHARGED;
        else
                battery_state = DKP_SOURCE_STATE_EMPTY;

        /* set the old status */
        source->priv->battery_energy_old = source->priv->battery_energy;
        g_get_current_time (&source->priv->battery_energy_old_timespec);

        /* we changed state */
        if (source->priv->battery_state != battery_state) {
                source->priv->battery_energy_old = -1;
                source->priv->battery_state = battery_state;
        }

        g_free (status);
        return TRUE;
}

static gboolean
_poll_battery (DkpSource *source)
{
        g_warning ("No updates on source %s for 30 seconds; forcing update", source->priv->native_path);
        source->priv->poll_timer_id = 0;
        update (source);
        emit_changed (source);
        return FALSE;
}

static gboolean
update (DkpSource *source)
{
        gboolean ret;

        if (source->priv->poll_timer_id > 0) {
                g_source_remove (source->priv->poll_timer_id);
                source->priv->poll_timer_id = 0;
        }

        g_get_current_time (&(source->priv->update_time));

        switch (source->priv->type) {
        case DKP_SOURCE_TYPE_LINE_POWER:
                ret = update_line_power (source);
                break;
        case DKP_SOURCE_TYPE_BATTERY:

                ret = update_battery (source);

                /* Seems that we don't get change uevents from the
                 * kernel on some BIOS types; set up a timer to poll
                 *
                 * TODO: perhaps only do this if we do not get frequent updates.
                 */
                source->priv->poll_timer_id = g_timeout_add_seconds (30, (GSourceFunc) _poll_battery, source);

                break;
        default:
                g_assert_not_reached ();
                break;
        }

        return ret;
}

/*--------------------------------------------------------------------------------------------------------------*/

gboolean
dkp_source_refresh (DkpSource     *power_source,
                             DBusGMethodInvocation *context)
{
        update (power_source);
        dbus_g_method_return (context);
        return TRUE;
}
