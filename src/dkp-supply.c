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
#include <math.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n-lib.h>
#include <glib-object.h>
#include <devkit-gobject/devkit-gobject.h>

#include "sysfs-utils.h"
#include "egg-debug.h"

#include "dkp-enum.h"
#include "dkp-supply.h"

#define DKP_SUPPLY_REFRESH_TIMEOUT	10L

struct DkpSupplyPrivate
{
	guint			 poll_timer_id;
	gboolean		 has_coldplug_values;
	gdouble			 energy_old;
	GTimeVal		 energy_old_timespec;
};

static void	dkp_supply_class_init	(DkpSupplyClass	*klass);

G_DEFINE_TYPE (DkpSupply, dkp_supply, DKP_TYPE_DEVICE)
#define DKP_SUPPLY_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), DKP_TYPE_SUPPLY, DkpSupplyPrivate))

static gboolean		 dkp_supply_refresh	 	(DkpDevice *device);

/**
 * dkp_supply_refresh_line_power:
 **/
static gboolean
dkp_supply_refresh_line_power (DkpSupply *supply)
{
	DkpDevice *device = DKP_DEVICE (supply);
	DevkitDevice *d;
	const gchar *native_path;

	d = dkp_device_get_d (device);
	if (d == NULL)
		egg_error ("could not get device");

	/* force true */
	g_object_set (device, "power-supply", TRUE, NULL);

	/* get new AC value */
	native_path = devkit_device_get_native_path (d);
	g_object_set (device, "online", sysfs_get_int (native_path, "online"), NULL);

	return TRUE;
}

/**
 * dkp_supply_reset_values:
 **/
static void
dkp_supply_reset_values (DkpSupply *supply)
{
	DkpDevice *device = DKP_DEVICE (supply);

	supply->priv->has_coldplug_values = FALSE;
	supply->priv->energy_old = 0;
	supply->priv->energy_old_timespec.tv_sec = 0;

	/* reset to default */
	g_object_set (device,
		      "vendor", NULL,
		      "model", NULL,
		      "serial", NULL,
		      "update-time", 0,
		      "power-supply", FALSE,
		      "online", FALSE,
		      "energy", 0.0,
		      "is-present", FALSE,
		      "is-rechargeable", FALSE,
		      "has-history", FALSE,
		      "has-statistics", FALSE,
		      "state", NULL,
		      "capacity", 0.0,
		      "energy-empty", 0.0,
		      "energy-full", 0.0,
		      "energy-full-design", 0.0,
		      "energy-rate", 0.0,
		      "voltage", 0.0,
		      "time-to-empty", 0,
		      "time-to-full", 0,
		      "percentage", 0,
		      "technology", NULL,
		      NULL);
}

/**
 * dkp_supply_get_on_battery:
 **/
static gboolean
dkp_supply_get_on_battery (DkpDevice *device, gboolean *on_battery)
{
	DkpSupply *supply = DKP_SUPPLY (device);
	DkpDeviceType type;
	DkpDeviceState state;
	gboolean is_present;

	g_return_val_if_fail (DKP_IS_SUPPLY (supply), FALSE);
	g_return_val_if_fail (on_battery != NULL, FALSE);

	g_object_get (device,
		      "type", &type,
		      "state", &state,
		      "is-present", &is_present,
		      NULL);

	if (type != DKP_DEVICE_TYPE_BATTERY)
		return FALSE;
	if (!is_present)
		return FALSE;

	*on_battery = (state == DKP_DEVICE_STATE_DISCHARGING);
	return TRUE;
}

/**
 * dkp_supply_get_low_battery:
 **/
static gboolean
dkp_supply_get_low_battery (DkpDevice *device, gboolean *low_battery)
{
	gboolean ret;
	gboolean on_battery;
	DkpSupply *supply = DKP_SUPPLY (device);
	guint percentage;

	g_return_val_if_fail (DKP_IS_SUPPLY (supply), FALSE);
	g_return_val_if_fail (low_battery != NULL, FALSE);

	/* reuse the common checks */
	ret = dkp_supply_get_on_battery (device, &on_battery);
	if (!ret)
		return FALSE;

	/* shortcut */
	if (!on_battery) {
		*low_battery = FALSE;
		return TRUE;
	}

	g_object_get (device, "percentage", &percentage, NULL);
	*low_battery = (percentage < 10);
	return TRUE;
}

/**
 * dkp_supply_calculate_rate:
 **/
static void
dkp_supply_calculate_rate (DkpSupply *supply)
{
	guint time;
	gdouble energy;
	gdouble energy_rate;
	GTimeVal now;
	DkpDevice *device = DKP_DEVICE (supply);

	g_object_get (device, "energy", &energy, NULL);

	if (energy < 0)
		return;

	if (supply->priv->energy_old < 0)
		return;

	if (supply->priv->energy_old == energy)
		return;

	/* get the time difference */
	g_get_current_time (&now);
	time = now.tv_sec - supply->priv->energy_old_timespec.tv_sec;

	if (time == 0)
		return;

	/* get the difference in charge */
	energy = supply->priv->energy_old - energy;
	if (energy < 0.1)
		return;

	/* probably okay */
	energy_rate = energy * 3600 / time;
	g_object_set (device, "energy-rate", energy_rate, NULL);
}

/**
 * dkp_supply_convert_device_technology:
 **/
static DkpDeviceTechnology
dkp_supply_convert_device_technology (const gchar *type)
{
	if (type == NULL)
		return DKP_DEVICE_TECHNOLOGY_UNKNOWN;
	/* every case combination of Li-Ion is commonly used.. */
	if (strcasecmp (type, "li-ion") == 0 ||
	    strcasecmp (type, "lion") == 0)
		return DKP_DEVICE_TECHNOLOGY_LITHIUM_ION;
	if (strcasecmp (type, "pb") == 0 ||
	    strcasecmp (type, "pbac") == 0)
		return DKP_DEVICE_TECHNOLOGY_LEAD_ACID;
	if (strcasecmp (type, "lip") == 0 ||
	    strcasecmp (type, "lipo") == 0)
		return DKP_DEVICE_TECHNOLOGY_LITHIUM_POLYMER;
	if (strcasecmp (type, "nimh") == 0)
		return DKP_DEVICE_TECHNOLOGY_NICKEL_METAL_HYDRIDE;
	if (strcasecmp (type, "lifo") == 0)
		return DKP_DEVICE_TECHNOLOGY_LITHIUM_IRON_PHOSPHATE;
	return DKP_DEVICE_TECHNOLOGY_UNKNOWN;
}

/**
 * dkp_supply_refresh_battery:
 *
 * Return value: TRUE if we changed
 **/
static gboolean
dkp_supply_refresh_battery (DkpSupply *supply)
{
	gchar *status = NULL;
	gchar *technology_native;
	gboolean ret = TRUE;
	gdouble voltage_design;
	DkpDeviceState old_state;
	DkpDeviceState state;
	DkpDevice *device = DKP_DEVICE (supply);
	const gchar *native_path;
	DevkitDevice *d;
	gboolean is_present;
	gdouble energy;
	gdouble energy_full;
	gdouble energy_full_design;
	gdouble energy_rate;
	gdouble capacity;
	gdouble percentage;
	gdouble voltage;
	guint64 time_to_empty;
	guint64 time_to_full;

	d = dkp_device_get_d (device);
	if (d == NULL)
		egg_error ("could not get device");

	native_path = devkit_device_get_native_path (d);

	/* have we just been removed? */
	is_present = sysfs_get_bool (native_path, "present");
	g_object_set (device, "is-present", is_present, NULL);
	if (!is_present) {
		dkp_supply_reset_values (supply);
		goto out;
	}

	/* get the currect charge */
	energy = sysfs_get_double (native_path, "energy_now") / 1000000.0;
	if (energy == 0)
		energy = sysfs_get_double (native_path, "energy_avg") / 1000000.0;

	/* used to convert A to W later */
	voltage_design = sysfs_get_double (native_path, "voltage_max_design") / 1000000.0;
	if (voltage_design < 1.00) {
		voltage_design = sysfs_get_double (native_path, "voltage_min_design") / 1000000.0;
		if (voltage_design < 1.00) {
			egg_debug ("using present voltage as design voltage");
			voltage_design = sysfs_get_double (native_path, "voltage_present") / 1000000.0;
		}
	}

	/* initial values */
	if (!supply->priv->has_coldplug_values) {

		/* when we add via sysfs power_supply class then we know this is true */
		g_object_set (device, "power-supply", TRUE, NULL);

		/* the ACPI spec is bad at defining battery type constants */
		technology_native = g_strstrip (sysfs_get_string (native_path, "technology"));
		g_object_set (device, "technology", dkp_supply_convert_device_technology (technology_native), NULL);
		g_free (technology_native);

		g_object_set (device,
			      "vendor", g_strstrip (sysfs_get_string (native_path, "manufacturer")),
			      "model", g_strstrip (sysfs_get_string (native_path, "model_name")),
			      "serial", g_strstrip (sysfs_get_string (native_path, "serial_number")),
			      "is-rechargeable", TRUE, /* assume true for laptops */
			      "has-history", TRUE,
			      "has-statistics", TRUE,
			      NULL);

		/* these don't change at runtime */
		energy_full = sysfs_get_double (native_path, "energy_full") / 1000000.0;
		energy_full_design = sysfs_get_double (native_path, "energy_full_design") / 1000000.0;

		/* convert charge to energy */
		if (energy == 0) {
			energy_full = sysfs_get_double (native_path, "charge_full") / 1000000.0;
			energy_full_design = sysfs_get_double (native_path, "charge_full_design") / 1000000.0;
			energy_full *= voltage_design;
			energy_full_design *= voltage_design;
		}

		/* the last full should not be bigger than the design */
		if (energy_full > energy_full_design)
			egg_warning ("energy_full (%f) is greater than energy_full_design (%f)",
				     energy_full, energy_full_design);

		/* some systems don't have this */
		if (energy_full < 0.01) {
			egg_warning ("correcting energy_full (%f) using energy_full_design (%f)",
				     energy_full, energy_full_design);
			energy_full = energy_full_design;
		}

		/* calculate how broken our battery is */
		if (energy_full > 0) {
			capacity = (energy_full / energy_full_design) * 100.0f;
			if (capacity < 0)
				capacity = 0.0;
			if (capacity > 100.0)
				capacity = 100.0;
		}
		g_object_set (device, "capacity", capacity, NULL);

		/* we only coldplug once, as these values will never change */
		supply->priv->has_coldplug_values = TRUE;
	} else {
		/* get the old full */
		g_object_get (device,
			      "energy-full", &energy_full,
			      "energy-full-design", &energy_full_design,
			      NULL);
	}

	status = g_strstrip (sysfs_get_string (native_path, "status"));
	if (strcasecmp (status, "charging") == 0)
		state = DKP_DEVICE_STATE_CHARGING;
	else if (strcasecmp (status, "discharging") == 0)
		state = DKP_DEVICE_STATE_DISCHARGING;
	else if (strcasecmp (status, "full") == 0)
		state = DKP_DEVICE_STATE_FULLY_CHARGED;
	else if (strcasecmp (status, "empty") == 0)
		state = DKP_DEVICE_STATE_EMPTY;
	else {
		egg_warning ("unknown status string: %s", status);
		state = DKP_DEVICE_STATE_UNKNOWN;
	}

	/* get rate; it seems odd as it's either in uVh or uWh */
	energy_rate = fabs (sysfs_get_double (native_path, "current_now") / 1000000.0);

	/* convert charge to energy */
	if (energy == 0) {
		energy = sysfs_get_double (native_path, "charge_now") / 1000000.0;
		if (energy == 0)
			energy = sysfs_get_double (native_path, "charge_avg") / 1000000.0;
		energy *= voltage_design;
		energy_rate *= voltage_design;
	}

	/* some batteries don't update last_full attribute */
	if (energy > energy_full) {
		egg_warning ("energy %f bigger than full %f", energy, energy_full);
		energy_full = energy;
	}

	/* present voltage */
	voltage = sysfs_get_double (native_path, "voltage_now") / 1000000.0;
	if (voltage == 0)
		voltage = sysfs_get_double (native_path, "voltage_avg") / 1000000.0;

	/* ACPI gives out the special 'Ones' value for rate when it's unable
	 * to calculate the true rate. We should set the rate zero, and wait
	 * for the BIOS to stabilise. */
	if (energy_rate == 0xffff)
		energy_rate = 0;

	/* sanity check to less than 100W */
	if (energy_rate > 100*1000)
		energy_rate = 0;

	/* the hardware reporting failed -- try to calculate this */
	if (energy_rate < 0)
		dkp_supply_calculate_rate (supply);

	/* get a precise percentage */
	percentage = 100.0 * energy / energy_full;
	if (percentage < 0)
		percentage = 0;
	if (percentage > 100.0)
		percentage = 100.0;

	/* calculate a quick and dirty time remaining value */
	time_to_empty = 0;
	time_to_full = 0;
	if (energy_rate > 0) {
		if (state == DKP_DEVICE_STATE_DISCHARGING)
			time_to_empty = 3600 * (energy / energy_rate);
		else if (state == DKP_DEVICE_STATE_CHARGING)
			time_to_full = 3600 * ((energy_full - energy) / energy_rate);
		/* TODO: need to factor in battery charge metrics */
	}

	/* check the remaining time is under a set limit, to deal with broken
	   primary batteries rate */
	if (time_to_empty > (20 * 60 * 60))
		time_to_empty = 0;
	if (time_to_full > (20 * 60 * 60))
		time_to_full = 0;

	/* set the old status */
	supply->priv->energy_old = energy;
	g_get_current_time (&supply->priv->energy_old_timespec);

	/* we changed state */
	g_object_get (device, "state", &old_state, NULL);
	if (old_state != state)
		supply->priv->energy_old = 0;

	g_object_set (device,
		      "energy", energy,
		      "energy-full", energy_full,
		      "energy-full-design", energy_full_design,
		      "energy-rate", energy_rate,
		      "percentage", percentage,
		      "state", state,
		      "voltage", voltage,
		      "time-to-empty", time_to_empty,
		      "time-to-full", time_to_full,
		      NULL);

out:
	g_free (status);
	return ret;
}

/**
 * dkp_supply_poll_battery:
 **/
static gboolean
dkp_supply_poll_battery (DkpSupply *supply)
{
	gboolean ret;
	DkpDevice *device = DKP_DEVICE (supply);

	egg_debug ("No updates on supply %s for 30 seconds; forcing update", dkp_device_get_object_path (device));
	supply->priv->poll_timer_id = 0;
	ret = dkp_supply_refresh (device);
	if (ret)
		dkp_device_emit_changed (device);
	return FALSE;
}

/**
 * dkp_supply_coldplug:
 **/
static gboolean
dkp_supply_coldplug (DkpDevice *device)
{
	DkpSupply *supply = DKP_SUPPLY (device);
	DevkitDevice *d;
	const gchar *native_path;

	dkp_supply_reset_values (supply);

	/* detect what kind of device we are */
	d = dkp_device_get_d (device);
	if (d == NULL)
		egg_error ("could not get device");

	native_path = devkit_device_get_native_path (d);
	if (native_path == NULL)
		egg_error ("could not get native path");

	if (sysfs_file_exists (native_path, "online")) {
		g_object_set (device, "type", DKP_DEVICE_TYPE_LINE_POWER, NULL);
	} else {
		/* this is correct, UPS and CSR are not in the kernel */
		g_object_set (device, "type", DKP_DEVICE_TYPE_BATTERY, NULL);
	}

	/* coldplug values */
	dkp_supply_refresh (device);

	return TRUE;
}

/**
 * dkp_supply_refresh:
 **/
static gboolean
dkp_supply_refresh (DkpDevice *device)
{
	gboolean ret;
	GTimeVal time;
	DkpSupply *supply = DKP_SUPPLY (device);
	DkpDeviceType type;
	DkpDeviceState state;

	if (supply->priv->poll_timer_id > 0) {
		g_source_remove (supply->priv->poll_timer_id);
		supply->priv->poll_timer_id = 0;
	}

	g_get_current_time (&time);
	g_object_set (device, "update-time", (guint64) time.tv_sec, NULL);
	g_object_get (device, "type", &type, NULL);
	switch (type) {
	case DKP_DEVICE_TYPE_LINE_POWER:
		ret = dkp_supply_refresh_line_power (supply);
		break;
	case DKP_DEVICE_TYPE_BATTERY:
		ret = dkp_supply_refresh_battery (supply);

		/* Seems that we don't get change uevents from the
		 * kernel on some BIOS types; set up a timer to poll
		 * if we are charging or discharging */
		g_object_get (device, "state", &state, NULL);
		if (state == DKP_DEVICE_STATE_CHARGING ||
		    state == DKP_DEVICE_STATE_DISCHARGING)
			supply->priv->poll_timer_id =
				g_timeout_add_seconds (DKP_SUPPLY_REFRESH_TIMEOUT,
						       (GSourceFunc) dkp_supply_poll_battery, supply);
		break;
	default:
		g_assert_not_reached ();
		break;
	}
	return ret;
}

/**
 * dkp_supply_init:
 **/
static void
dkp_supply_init (DkpSupply *supply)
{
	supply->priv = DKP_SUPPLY_GET_PRIVATE (supply);
}

/**
 * dkp_supply_finalize:
 **/
static void
dkp_supply_finalize (GObject *object)
{
	DkpSupply *supply;

	g_return_if_fail (object != NULL);
	g_return_if_fail (DKP_IS_SUPPLY (object));

	supply = DKP_SUPPLY (object);
	g_return_if_fail (supply->priv != NULL);

	if (supply->priv->poll_timer_id > 0)
		g_source_remove (supply->priv->poll_timer_id);

	G_OBJECT_CLASS (dkp_supply_parent_class)->finalize (object);
}

/**
 * dkp_supply_class_init:
 **/
static void
dkp_supply_class_init (DkpSupplyClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	DkpDeviceClass *device_class = DKP_DEVICE_CLASS (klass);

	object_class->finalize = dkp_supply_finalize;
	device_class->get_on_battery = dkp_supply_get_on_battery;
	device_class->get_low_battery = dkp_supply_get_low_battery;
	device_class->coldplug = dkp_supply_coldplug;
	device_class->refresh = dkp_supply_refresh;

	g_type_class_add_private (klass, sizeof (DkpSupplyPrivate));
}

/**
 * dkp_supply_new:
 **/
DkpSupply *
dkp_supply_new (void)
{
	return g_object_new (DKP_TYPE_SUPPLY, NULL);
}

