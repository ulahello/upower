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

#include "config.h"

#include <string.h>
#include <math.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gprintf.h>
#include <glib/gi18n-lib.h>
#include <glib-object.h>
#include <gudev/gudev.h>

#include "up-config.h"
#include "up-common.h"
#include "up-types.h"
#include "up-constants.h"
#include "up-device-supply-battery.h"

/* For up_device_supply_get_state */
#include "up-device-supply.h"

enum {
	PROP_0,
	PROP_IGNORE_SYSTEM_PERCENTAGE
};

struct _UpDeviceSupplyBattery
{
	UpDeviceBattery		 parent;
	gboolean		 has_coldplug_values;
	gboolean		 coldplug_units;
	gdouble			*energy_old;
	guint			 energy_old_first;
	gdouble			 rate_old;
	gboolean		 shown_invalid_voltage_warning;
	gboolean		 ignore_system_percentage;
};

G_DEFINE_TYPE (UpDeviceSupplyBattery, up_device_supply_battery, UP_TYPE_DEVICE_BATTERY)

static gdouble
up_device_supply_battery_get_design_voltage (UpDeviceSupplyBattery *self,
					     GUdevDevice *native)
{
	gdouble voltage;
	const gchar *device_type = NULL;

	/* design maximum */
	voltage = g_udev_device_get_sysfs_attr_as_double_uncached (native, "voltage_max_design") / 1000000.0;
	if (voltage > 1.00f) {
		g_debug ("using max design voltage");
		return voltage;
	}

	/* design minimum */
	voltage = g_udev_device_get_sysfs_attr_as_double_uncached (native, "voltage_min_design") / 1000000.0;
	if (voltage > 1.00f) {
		g_debug ("using min design voltage");
		return voltage;
	}

	/* current voltage, alternate form */
	voltage = g_udev_device_get_sysfs_attr_as_double_uncached (native, "voltage_now") / 1000000.0;
	if (voltage > 1.00f) {
		g_debug ("using present voltage (alternate)");
		return voltage;
	}

	/* is this a USB device? */
	device_type = g_udev_device_get_sysfs_attr (native, "type");
	if (device_type != NULL && g_ascii_strcasecmp (device_type, "USB") == 0) {
		g_debug ("USB device, so assuming 5v");
		voltage = 5.0f;
		return voltage;
	}

	/* no valid value found; display a warning the first time for each
	 * device */
	if (!self->shown_invalid_voltage_warning) {
		self->shown_invalid_voltage_warning = TRUE;
		g_warning ("no valid voltage value found for device %s, assuming 10V",
			   g_udev_device_get_sysfs_path (native));
	}
	/* completely guess, to avoid getting zero values */
	g_debug ("no voltage values for device %s, using 10V as approximation",
		 g_udev_device_get_sysfs_path (native));
	voltage = 10.0f;

	return voltage;
}

static char*
get_sysfs_attr_uncached (GUdevDevice *native, const gchar *key)
{
	g_autofree char *value = NULL;

	/* get value, and strip to remove spaces */
	value = g_strdup (g_udev_device_get_sysfs_attr_uncached (native, key));
	if (!value)
		return NULL;

	g_strstrip (value);
	if (value[0] == '\0')
		return NULL;

	return g_steal_pointer (&value);
}

static gboolean
up_device_supply_battery_convert_to_double (const gchar *str_value, gdouble *value)
{
	gdouble conv_value;
	gchar *end = NULL;

	if (str_value == NULL || value == NULL)
		return FALSE;

	conv_value = g_ascii_strtod (str_value, &end);
	if (end == str_value || conv_value < 0.0 || conv_value > 100.0)
		return FALSE;

	*value = conv_value;

	return TRUE;
}

static gboolean
up_device_supply_battery_get_charge_control_limits (GUdevDevice *native, UpBatteryInfo *info)
{
	const gchar *charge_limit;
	g_auto(GStrv) pairs = NULL;
	gdouble charge_control_start_threshold;
	gdouble charge_control_end_threshold;

	charge_limit = g_udev_device_get_property (native, "CHARGE_LIMIT");
	if (charge_limit == NULL)
		return FALSE;

	pairs = g_strsplit (charge_limit, ",", 0);
	if (g_strv_length (pairs) != 2) {
		g_warning("Could not parse CHARGE_LIMIT, expected 'number,number', got '%s'", charge_limit);
		return FALSE;
	}

	if (g_strcmp0 (pairs[0], "_") == 0) {
		g_debug ("charge_control_start_threshold is disabled");
		charge_control_start_threshold = G_MAXUINT;
	} else if (!up_device_supply_battery_convert_to_double (pairs[0], &charge_control_start_threshold)) {
		g_warning ("failed to convert charge_control_start_threshold: %s", pairs[0]);
		return FALSE;
	}

	if (g_strcmp0 (pairs[1], "_") == 0) {
		g_debug ("charge_control_end_threshold is disabled");
		charge_control_end_threshold = G_MAXUINT;
	} else if (!up_device_supply_battery_convert_to_double (pairs[1], &charge_control_end_threshold)) {
		g_warning ("failed to convert charge_control_start_threshold: %s", pairs[0]);
		return FALSE;
	}

	info->charge_control_start_threshold = charge_control_start_threshold;
	info->charge_control_end_threshold = charge_control_end_threshold;

	return TRUE;
}

static gboolean
up_device_supply_battery_refresh (UpDevice *device,
				  UpRefreshReason reason)
{
	UpDeviceSupplyBattery *self = UP_DEVICE_SUPPLY_BATTERY (device);
	UpDeviceBattery *battery = UP_DEVICE_BATTERY (device);
	GUdevDevice *native;
	UpBatteryInfo info = { 0 };
	UpBatteryValues values = { 0 };

	native = G_UDEV_DEVICE (up_device_get_native (device));

	/*
	 * Reload battery information.
	 * NOTE: If we assume that a udev event is guaranteed to happen, then
	 *       we can restrict this to updates other than UP_REFRESH_POLL.
	 * NOTE: Only energy.full and cycle_count can change for a battery.
	 */
	info.present = TRUE;
	if (g_udev_device_has_sysfs_attr (native, "present"))
		info.present = g_udev_device_get_sysfs_attr_as_boolean_uncached (native, "present");
	if (!info.present) {
		up_device_battery_update_info (battery, &info);
		return TRUE;
	}

	info.vendor = up_make_safe_string (get_sysfs_attr_uncached (native, "manufacturer"));
	info.model = up_make_safe_string (get_sysfs_attr_uncached (native, "model_name"));
	info.serial = up_make_safe_string (get_sysfs_attr_uncached (native, "serial_number"));

	info.voltage_design = up_device_supply_battery_get_design_voltage (self, native);
	info.charge_cycles = g_udev_device_get_sysfs_attr_as_int_uncached (native, "cycle_count");

	info.units = UP_BATTERY_UNIT_ENERGY;
	info.energy.full = g_udev_device_get_sysfs_attr_as_double_uncached (native, "energy_full") / 1000000.0;
	info.energy.design = g_udev_device_get_sysfs_attr_as_double_uncached (native, "energy_full_design") / 1000000.0;

	/* Assume we couldn't read anything if energy.full is extremely small */
	if (info.energy.full < 0.01) {
		info.units = UP_BATTERY_UNIT_CHARGE;
		info.energy.full = g_udev_device_get_sysfs_attr_as_double_uncached (native, "charge_full") / 1000000.0;
		info.energy.design = g_udev_device_get_sysfs_attr_as_double_uncached (native, "charge_full_design") / 1000000.0;
	}
	info.technology = up_convert_device_technology (get_sysfs_attr_uncached (native, "technology"));

	if (up_device_supply_battery_get_charge_control_limits (native, &info)) {
		info.charge_control_supported = TRUE;
		info.charge_control_enabled = FALSE;
	} else {
		info.charge_control_enabled = FALSE;
		info.charge_control_supported = FALSE;
	}

	/* NOTE: We used to warn about full > design, but really that is perfectly fine to happen. */

	/* Update the battery information (will only fire events for actual changes) */
	up_device_battery_update_info (battery, &info);

	/*
	 * Load dynamic information.
	 */
	values.units = info.units;

	values.voltage = g_udev_device_get_sysfs_attr_as_double_uncached (native, "voltage_now") / 1000000.0;
	if (values.voltage < 0.01)
		values.voltage = g_udev_device_get_sysfs_attr_as_double_uncached (native, "voltage_avg") / 1000000.0;


	switch (values.units) {
	case UP_BATTERY_UNIT_CHARGE:
		/* QUIRK:
		 * Some batteries (Nexus 7?) may report a separate energy_now.
		 * This appears to be more accurate (as it takes into account
		 * the dropping voltage). However, we ignore this here for
		 * consistency (but we used to read it in the past).
		 *
		 * See https://bugs.freedesktop.org/show_bug.cgi?id=60104#c2
		 * which's reports energy_now of 15.05 Wh while our calculation
		 * will be ~16.4Wh by multiplying charge with voltage).
		 */
		values.energy.rate = fabs (g_udev_device_get_sysfs_attr_as_double_uncached (native, "current_now") / 1000000.0);
		values.energy.cur = fabs (g_udev_device_get_sysfs_attr_as_double_uncached (native, "charge_now") / 1000000.0);
		break;
	case UP_BATTERY_UNIT_ENERGY:
		values.energy.rate = fabs (g_udev_device_get_sysfs_attr_as_double_uncached (native, "power_now") / 1000000.0);
		values.energy.cur = fabs (g_udev_device_get_sysfs_attr_as_double_uncached (native, "energy_now") / 1000000.0);
		if (values.energy.cur < 0.01)
			values.energy.cur = g_udev_device_get_sysfs_attr_as_double_uncached (native, "energy_avg") / 1000000.0;

		/* Legacy case: If we have energy units but no power_now, then current_now is in uW. */
		if (values.energy.rate < 0)
			values.energy.rate = fabs (g_udev_device_get_sysfs_attr_as_double_uncached (native, "current_now") / 1000000.0);
		break;
	default:
		g_assert_not_reached ();
	}

	/* NOTE:
	 * The old code tried to special case the 0xffff ACPI value of the energy rate.
	 * That doesn't really make any sense after doing the floating point math.
	 */

	if (!self->ignore_system_percentage) {
		values.percentage = g_udev_device_get_sysfs_attr_as_double_uncached (native, "capacity");
		values.percentage = CLAMP(values.percentage, 0.0f, 100.0f);
	}

	/* For some battery solutions, for example axp20x-battery, the kernel reports
	 * status = charging but the battery actually discharges when connecting a
	 * charger. Upower reports the battery is "discharging" when current_now is
	 * found and is a negative value.*/
	if (g_udev_device_get_sysfs_attr_as_double_uncached (native, "current_now") < 0.0)
		values.state = UP_DEVICE_STATE_DISCHARGING;
	else
		values.state = up_device_supply_get_state (native);

	values.temperature = g_udev_device_get_sysfs_attr_as_double_uncached (native, "temp") / 10.0;

	up_device_battery_report (battery, &values, reason);

	return TRUE;
}

/**
 * up_device_supply_coldplug:
 *
 * Return %TRUE on success, %FALSE if we failed to get data and should be removed
 **/
static gboolean
up_device_supply_coldplug (UpDevice *device)
{
	GUdevDevice *native;
	const gchar *native_path;
	const gchar *scope;
	const gchar *type;

	/* detect what kind of device we are */
	native = G_UDEV_DEVICE (up_device_get_native (device));
	native_path = g_udev_device_get_sysfs_path (native);
	if (native_path == NULL) {
		g_warning ("could not get native path for %p", device);
		return FALSE;
	}

	/* try to work out if the device is powering the system */
	scope = g_udev_device_get_sysfs_attr (native, "scope");
	if (scope != NULL && g_ascii_strcasecmp (scope, "device") == 0)
		return FALSE;

	/* Complain about a non-system scope, while accepting no scope information */
	if (scope != NULL && g_ascii_strcasecmp (scope, "system") != 0)
		g_warning ("Assuming system scope even though scope is %s", scope);

	/* type must be a battery. */
	type = g_udev_device_get_sysfs_attr (native, "type");
	if (!type || g_ascii_strcasecmp (type, "battery") != 0)
		return FALSE;

	return TRUE;
}

static void
up_device_supply_battery_init (UpDeviceSupplyBattery *self)
{
}

static void
up_device_supply_battery_set_property (GObject        *object,
				       guint           property_id,
				       const GValue   *value,
				       GParamSpec     *pspec)
{
	UpDeviceSupplyBattery *self = UP_DEVICE_SUPPLY_BATTERY (object);

	switch (property_id) {
	case PROP_IGNORE_SYSTEM_PERCENTAGE:
		self->ignore_system_percentage = g_value_get_boolean (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
	}
}

static void
up_device_supply_battery_get_property (GObject        *object,
				       guint           property_id,
				       GValue         *value,
				       GParamSpec     *pspec)
{
	UpDeviceSupplyBattery *self = UP_DEVICE_SUPPLY_BATTERY (object);

	switch (property_id) {
	case PROP_IGNORE_SYSTEM_PERCENTAGE:
		g_value_set_flags (value, self->ignore_system_percentage);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
	}
}

static char *
up_device_supply_device_path (GUdevDevice *device)
{
	const char *root;

	root = g_getenv ("UMOCKDEV_DIR");
	if (!root || *root == '\0') {
		return g_strdup (g_udev_device_get_sysfs_path (device));
	}

	return g_build_filename (root,
				 g_udev_device_get_sysfs_path (device),
				 NULL);
}

static gboolean
up_device_supply_battery_set_battery_charge_thresholds(UpDevice *device, guint start, guint end, GError **error) {
	guint err_count = 0;
	GUdevDevice *native;
	g_autofree gchar *native_path = NULL;
	g_autofree gchar *start_filename = NULL;
	g_autofree gchar *end_filename = NULL;
	g_autoptr (GString) start_str = g_string_new (NULL);
	g_autoptr (GString) end_str = g_string_new (NULL);

	native = G_UDEV_DEVICE (up_device_get_native (device));
	native_path = up_device_supply_device_path (native);
	start_filename = g_build_filename (native_path, "charge_control_start_threshold", NULL);
	end_filename = g_build_filename (native_path, "charge_control_end_threshold", NULL);

	if (start != G_MAXUINT) {
		g_string_printf (start_str, "%d", CLAMP (start, 0, 100));
		if (!g_file_set_contents_full (start_filename, start_str->str, start_str->len,
					  G_FILE_SET_CONTENTS_ONLY_EXISTING, 0644, error)) {
			err_count++;
		}
	} else {
		g_debug ("Ignore charge_control_start_threshold setting");
	}

	if (end != G_MAXUINT) {
		g_string_printf (end_str, "%d", CLAMP (end, 0, 100));
		if (!g_file_set_contents_full (end_filename, end_str->str, end_str->len,
					  G_FILE_SET_CONTENTS_ONLY_EXISTING, 0644, error)) {
			err_count++;
		}
	} else {
		g_debug ("Ignore charge_control_end_threshold setting");
	}

	if (err_count == 2) {
		g_set_error_literal (error, G_IO_ERROR,
				     G_IO_ERROR_FAILED, "Failed to set charge control thresholds");
		return FALSE;
	}

	return TRUE;
}

static void
up_device_supply_battery_class_init (UpDeviceSupplyBatteryClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	UpDeviceClass *device_class = UP_DEVICE_CLASS (klass);
	UpDeviceBatteryClass *battery_class = UP_DEVICE_BATTERY_CLASS (klass);

	object_class->set_property = up_device_supply_battery_set_property;
	object_class->get_property = up_device_supply_battery_get_property;
	device_class->coldplug = up_device_supply_coldplug;
	device_class->refresh = up_device_supply_battery_refresh;
	battery_class->set_battery_charge_thresholds = up_device_supply_battery_set_battery_charge_thresholds;

	g_object_class_install_property (object_class, PROP_IGNORE_SYSTEM_PERCENTAGE,
					 g_param_spec_boolean ("ignore-system-percentage",
							       "Ignore system percentage",
							       "Ignore system provided battery percentage",
							       FALSE, G_PARAM_READWRITE));
}
