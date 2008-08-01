/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
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

#include <glib.h>
#include <glib-object.h>
#include <string.h>

#include "dkp-debug.h"
#include "dkp-enum.h"
#include "dkp-object.h"

/**
 * dkp_object_clear_internal:
 **/
static void
dkp_object_clear_internal (DkpObject *obj)
{
	obj->type = DKP_SOURCE_TYPE_UNKNOWN;
	obj->update_time = 0;
	obj->battery_energy = -1;
	obj->battery_energy_full = -1;
	obj->battery_energy_full_design = -1;
	obj->battery_energy_rate = -1;
	obj->battery_percentage = -1;
	obj->battery_capacity = -1;
	obj->battery_time_to_empty = -1;
	obj->battery_time_to_full = -1;
	obj->battery_state = DKP_SOURCE_STATE_UNKNOWN;
	obj->battery_technology = DKP_SOURCE_TECHNOLGY_UNKNOWN;
	obj->vendor = NULL;
	obj->model = NULL;
	obj->serial = NULL;
	obj->native_path = NULL;
	obj->line_power_online = FALSE;
	obj->battery_is_present = FALSE;
	obj->power_supply = FALSE;
	obj->battery_is_rechargeable = FALSE;
}

/**
 * dkp_object_collect_props:
 **/
static void
dkp_object_collect_props (const char *key, const GValue *value, DkpObject *obj)
{
	gboolean handled = TRUE;

	if (strcmp (key, "native-path") == 0)
		obj->native_path = g_strdup (g_value_get_string (value));
	else if (strcmp (key, "vendor") == 0)
		obj->vendor = g_strdup (g_value_get_string (value));
	else if (strcmp (key, "model") == 0)
		obj->model = g_strdup (g_value_get_string (value));
	else if (strcmp (key, "serial") == 0)
		obj->serial = g_strdup (g_value_get_string (value));
	else if (strcmp (key, "update-time") == 0)
		obj->update_time = g_value_get_uint64 (value);
	else if (strcmp (key, "type") == 0)
		obj->type = dkp_source_type_from_text (g_value_get_string (value));
	else if (strcmp (key, "line-power-online") == 0)
		obj->line_power_online = g_value_get_boolean (value);
	else if (strcmp (key, "battery-energy") == 0)
		obj->battery_energy = g_value_get_double (value);
	else if (strcmp (key, "battery-energy-empty") == 0)
		obj->battery_energy_empty = g_value_get_double (value);
	else if (strcmp (key, "battery-energy-full") == 0)
		obj->battery_energy_full = g_value_get_double (value);
	else if (strcmp (key, "battery-energy-full-design") == 0)
		obj->battery_energy_full_design = g_value_get_double (value);
	else if (strcmp (key, "battery-energy-rate") == 0)
		obj->battery_energy_rate = g_value_get_double (value);
	else if (strcmp (key, "battery-time-to-full") == 0)
		obj->battery_time_to_full = g_value_get_int64 (value);
	else if (strcmp (key, "battery-time-to-empty") == 0)
		obj->battery_time_to_empty = g_value_get_int64 (value);
	else if (strcmp (key, "battery-percentage") == 0)
		obj->battery_percentage = g_value_get_double (value);
	else if (strcmp (key, "battery-technology") == 0)
		obj->battery_technology = dkp_source_technology_from_text (g_value_get_string (value));
	else if (strcmp (key, "battery-is-present") == 0)
		obj->battery_is_present = g_value_get_boolean (value);
	else if (strcmp (key, "battery-is-rechargeable") == 0)
		obj->battery_is_rechargeable = g_value_get_boolean (value);
	else if (strcmp (key, "power-supply") == 0)
		obj->power_supply = g_value_get_boolean (value);
	else if (strcmp (key, "battery-capacity") == 0)
		obj->battery_capacity = g_value_get_double (value);
	else if (strcmp (key, "battery-state") == 0)
		obj->battery_state = dkp_source_state_from_text (g_value_get_string (value));
	else
		handled = FALSE;

	if (!handled)
		dkp_warning ("unhandled property '%s'", key);
}

/**
 * dkp_object_set_from_map:
 **/
gboolean
dkp_object_set_from_map	(DkpObject *obj, GHashTable *hash_table)
{
	g_hash_table_foreach (hash_table, (GHFunc) dkp_object_collect_props, obj);
	return TRUE;
}

/**
 * dkp_object_copy:
 **/
DkpObject *
dkp_object_copy (const DkpObject *cobj)
{
	DkpObject *obj;
	obj = g_new0 (DkpObject, 1);

	obj->type = cobj->type;
	obj->update_time = cobj->update_time;
	obj->battery_energy = cobj->battery_energy;
	obj->battery_energy_full = cobj->battery_energy_full;
	obj->battery_energy_full_design = cobj->battery_energy_full_design;
	obj->battery_energy_rate = cobj->battery_energy_rate;
	obj->battery_percentage = cobj->battery_percentage;
	obj->battery_capacity = cobj->battery_capacity;
	obj->battery_time_to_empty = cobj->battery_time_to_empty;
	obj->battery_time_to_full = cobj->battery_time_to_full;
	obj->battery_state = cobj->battery_state;
	obj->battery_technology = cobj->battery_technology;
	obj->vendor = g_strdup (cobj->vendor);
	obj->model = g_strdup (cobj->model);
	obj->serial = g_strdup (cobj->serial);
	obj->native_path = g_strdup (cobj->native_path);
	obj->line_power_online = cobj->line_power_online;
	obj->battery_is_present = cobj->battery_is_present;
	obj->power_supply = cobj->power_supply;
	obj->battery_is_rechargeable = cobj->battery_is_rechargeable;

	return obj;
}

/**
 * dkp_object_equal:
 **/
gboolean
dkp_object_equal (const DkpObject *obj1, const DkpObject *obj2)
{
	if (obj1->type == obj2->type &&
	    obj1->update_time == obj2->update_time &&
	    obj1->battery_energy == obj2->battery_energy &&
	    obj1->battery_energy_full == obj2->battery_energy_full &&
	    obj1->battery_energy_full_design == obj2->battery_energy_full_design &&
	    obj1->battery_energy_rate == obj2->battery_energy_rate &&
	    obj1->battery_percentage == obj2->battery_percentage &&
	    obj1->battery_capacity == obj2->battery_capacity &&
	    obj1->battery_time_to_empty == obj2->battery_time_to_empty &&
	    obj1->battery_time_to_full == obj2->battery_time_to_full &&
	    obj1->battery_state == obj2->battery_state &&
	    obj1->battery_technology == obj2->battery_technology &&
	    strcmp (obj1->vendor, obj2->vendor) == 0 &&
	    strcmp (obj1->model, obj2->model) == 0 &&
	    strcmp (obj1->serial, obj2->serial) == 0 &&
	    strcmp (obj1->native_path, obj2->native_path) == 0 &&
	    obj1->line_power_online == obj2->line_power_online &&
	    obj1->battery_is_present == obj2->battery_is_present &&
	    obj1->power_supply == obj2->power_supply &&
	    obj1->battery_is_rechargeable == obj2->battery_is_rechargeable)
		return TRUE;
	return FALSE;
}

/**
 * dkp_object_print:
 **/
gboolean
dkp_object_print (const DkpObject *obj)
{
	gboolean ret = TRUE;
	struct tm *time_tm;
	time_t t;
	gchar time_buf[256];

	/* get a human readable time */
	t = (time_t) obj->update_time;
	time_tm = localtime (&t);
	strftime (time_buf, sizeof time_buf, "%c", time_tm);

	g_print ("  native-path:          %s\n", obj->native_path);
	g_print ("  vendor:               %s\n", obj->vendor);
	g_print ("  model:                %s\n", obj->model);
	g_print ("  serial:               %s\n", obj->serial);
	g_print ("  power supply:         %s\n", obj->power_supply ? "yes" : "no");
	g_print ("  updated:              %s (%d seconds ago)\n", time_buf, (int) (time (NULL) - obj->update_time));
	if (obj->type == DKP_SOURCE_TYPE_BATTERY) {
		g_print ("  battery\n");
		g_print ("    present:             %s\n", obj->battery_is_present ? "yes" : "no");
		g_print ("    rechargeable:        %s\n", obj->battery_is_rechargeable ? "yes" : "no");
		g_print ("    state:               %s\n", dkp_source_state_to_text (obj->battery_state));
		g_print ("    energy:              %g Wh\n", obj->battery_energy);
		g_print ("    energy-empty:        %g Wh\n", obj->battery_energy_empty);
		g_print ("    energy-full:         %g Wh\n", obj->battery_energy_full);
		g_print ("    energy-full-design:  %g Wh\n", obj->battery_energy_full_design);
		g_print ("    energy-rate:         %g W\n", obj->battery_energy_rate);
		g_print ("    time to full:        ");
		if (obj->battery_time_to_full >= 0)
			g_print ("%d seconds\n", (int) obj->battery_time_to_full);
		else
			g_print ("unknown\n");
		g_print ("    time to empty:       ");
		if (obj->battery_time_to_empty >= 0)
			g_print ("%d seconds\n", (int) obj->battery_time_to_empty);
		else
			g_print ("unknown\n");
		g_print ("    percentage:          %g%%\n", obj->battery_percentage);
		g_print ("    capacity:            %g%%\n", obj->battery_capacity);
		g_print ("    technology:          %s\n", dkp_source_technology_to_text (obj->battery_technology));
	} else if (obj->type == DKP_SOURCE_TYPE_LINE_POWER) {
		g_print ("  line-power\n");
		g_print ("    online:             %s\n", obj->line_power_online ? "yes" : "no");
	} else {
		g_print ("  unknown power source type '%s'\n", dkp_source_type_to_text (obj->type));
		ret = FALSE;
	}
	return ret;
}

/**
 * dkp_object_new:
 **/
DkpObject *
dkp_object_new (void)
{
	DkpObject *obj;
	obj = g_new0 (DkpObject, 1);
	dkp_object_clear_internal (obj);
	return obj;
}

/**
 * dkp_object_clear:
 **/
gboolean
dkp_object_clear (DkpObject *obj)
{
	if (obj == NULL)
		return FALSE;
	dkp_object_free (obj);
	dkp_object_clear_internal (obj);
	return TRUE;
}

/**
 * dkp_object_free:
 **/
gboolean
dkp_object_free (DkpObject *obj)
{
	if (obj == NULL)
		return FALSE;
	g_free (obj->vendor);
	g_free (obj->model);
	g_free (obj->serial);
	g_free (obj->native_path);
	g_free (obj);
	return TRUE;
}

/**
 * dkp_object_get_id:
 **/
gchar *
dkp_object_get_id (DkpObject *obj)
{
	GString *string;
	gchar *id = NULL;

	/* only valid for devices supplying the system */
	if (!obj->power_supply)
		return id;

	/* only valid for batteries */
	if (obj->type != DKP_SOURCE_TYPE_BATTERY)
		return id;

	/* we don't have an ID if we are not present */
	if (!obj->battery_is_present)
		return id;

	string = g_string_new ("");

	/* in an ideal world, model-capacity-serial */
	if (obj->model != NULL && strlen (obj->model) > 2) {
		g_string_append (string, obj->model);
		g_string_append_c (string, '-');
	}
	if (obj->battery_energy_full_design > 0) {
		g_string_append_printf (string, "%i", (guint) obj->battery_energy_full_design);
		g_string_append_c (string, '-');
	}
	if (obj->serial != NULL && strlen (obj->serial) > 2) {
		g_string_append (string, obj->serial);
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

