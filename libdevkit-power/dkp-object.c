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
 * dkp_strequal:
 * @id1: the first item of text to test
 * @id2: the second item of text to test
 *
 * This function is a much safer way of doing strcmp as it checks for
 * NULL first, and returns boolean TRUE, not zero for success.
 *
 * Return value: %TRUE if the string are the same.
 **/
static gboolean
dkp_strequal (const gchar *id1, const gchar *id2)
{
	if (id1 == NULL && id2 == NULL)
		return TRUE;
	if (id1 == NULL || id2 == NULL)
		return FALSE;
	return (strcmp (id1, id2) == 0);
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
	    dkp_strequal (obj1->vendor, obj2->vendor) &&
	    dkp_strequal (obj1->model, obj2->model) &&
	    dkp_strequal (obj1->serial, obj2->serial) &&
	    dkp_strequal (obj1->native_path, obj2->native_path) &&
	    obj1->line_power_online == obj2->line_power_online &&
	    obj1->battery_is_present == obj2->battery_is_present &&
	    obj1->power_supply == obj2->power_supply &&
	    obj1->battery_is_rechargeable == obj2->battery_is_rechargeable)
		return TRUE;
	return FALSE;
}

/**
 * dkp_strzero:
 * @text: The text to check
 *
 * This function is a much safer way of doing "if (strlen (text) == 0))"
 * as it does not rely on text being NULL terminated. It's also much
 * quicker as it only checks the first byte rather than scanning the whole
 * string just to verify it's not zero length.
 *
 * Return value: %TRUE if the string was converted correctly
 **/
static gboolean
dkp_strzero (const gchar *text)
{
	if (text == NULL) {
		return TRUE;
	}
	if (text[0] == '\0') {
		return TRUE;
	}
	return FALSE;
}

/**
 * dkp_object_time_to_text:
 **/
static gchar *
dkp_object_time_to_text (gint seconds)
{
	gfloat value = seconds;

	if (value < 0)
		return g_strdup ("unknown");
	if (value < 60)
		return g_strdup_printf ("%.0f seconds", value);
	value /= 60.0;
	if (value < 60)
		return g_strdup_printf ("%.1f minutes", value);
	value /= 60.0;
	if (value < 60)
		return g_strdup_printf ("%.1f hours", value);
	value /= 24.0;
	return g_strdup_printf ("%.1f days", value);
}

/**
 * dkp_object_bool_to_text:
 **/
static const gchar *
dkp_object_bool_to_text (gboolean ret)
{
	return ret ? "yes" : "no";
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
	gchar *time_str;

	/* get a human readable time */
	t = (time_t) obj->update_time;
	time_tm = localtime (&t);
	strftime (time_buf, sizeof time_buf, "%c", time_tm);

	g_print ("  native-path:          %s\n", obj->native_path);
	if (!dkp_strzero (obj->vendor))
		g_print ("  vendor:               %s\n", obj->vendor);
	if (!dkp_strzero (obj->model))
		g_print ("  model:                %s\n", obj->model);
	if (!dkp_strzero (obj->serial))
		g_print ("  serial:               %s\n", obj->serial);
	g_print ("  power supply:         %s\n", dkp_object_bool_to_text (obj->power_supply));
	g_print ("  updated:              %s (%d seconds ago)\n", time_buf, (int) (time (NULL) - obj->update_time));
	if (obj->type == DKP_SOURCE_TYPE_BATTERY) {
		g_print ("  battery\n");
		g_print ("    present:             %s\n", dkp_object_bool_to_text (obj->battery_is_present));
		g_print ("    rechargeable:        %s\n", dkp_object_bool_to_text (obj->battery_is_rechargeable));
		g_print ("    state:               %s\n", dkp_source_state_to_text (obj->battery_state));
		g_print ("    energy:              %g Wh\n", obj->battery_energy);
		g_print ("    energy-empty:        %g Wh\n", obj->battery_energy_empty);
		g_print ("    energy-full:         %g Wh\n", obj->battery_energy_full);
		g_print ("    energy-full-design:  %g Wh\n", obj->battery_energy_full_design);
		g_print ("    energy-rate:         %g W\n", obj->battery_energy_rate);
		if (obj->battery_time_to_full >= 0) {
			time_str = dkp_object_time_to_text (obj->battery_time_to_full);
			g_print ("    time to full:        %s\n", time_str);
			g_free (time_str);
		}
		if (obj->battery_time_to_empty >= 0) {
			time_str = dkp_object_time_to_text (obj->battery_time_to_empty);
			g_print ("    time to empty:       %s\n", time_str);
			g_free (time_str);
		}
		g_print ("    percentage:          %g%%\n", obj->battery_percentage);
		g_print ("    capacity:            %g%%\n", obj->battery_capacity);
		g_print ("    technology:          %s\n", dkp_source_technology_to_text (obj->battery_technology));
	} else if (obj->type == DKP_SOURCE_TYPE_LINE_POWER) {
		g_print ("  line-power\n");
		g_print ("    online:             %s\n", dkp_object_bool_to_text (obj->line_power_online));
	} else {
		g_print ("  unknown power source type '%s'\n", dkp_source_type_to_text (obj->type));
		ret = FALSE;
	}
	return ret;
}

/**
 * dkp_object_diff:
 **/
gboolean
dkp_object_diff (const DkpObject *old, const DkpObject *obj)
{
	gchar *time_str;
	gchar *time_str_old;
	gboolean ret = TRUE;

	g_print ("  native-path:          %s\n", obj->native_path);
	if (!dkp_strequal (obj->vendor, old->vendor))
		g_print ("  vendor:               %s -> %s\n", old->vendor, obj->vendor);
	if (!dkp_strequal (obj->model, old->model))
		g_print ("  model:                %s -> %s\n", old->model, obj->model);
	if (!dkp_strequal (obj->serial, old->serial))
		g_print ("  serial:               %s -> %s\n", old->serial, obj->serial);

	if (obj->type == DKP_SOURCE_TYPE_BATTERY) {
		g_print ("  battery\n");
		if (old->battery_is_present != obj->battery_is_present)
			g_print ("    present:             %s -> %s\n",
				 dkp_object_bool_to_text (old->battery_is_present),
				 dkp_object_bool_to_text (obj->battery_is_present));
		if (old->battery_is_rechargeable != obj->battery_is_rechargeable)
			g_print ("    rechargeable:        %s -> %s\n",
				 dkp_object_bool_to_text (old->battery_is_rechargeable),
				 dkp_object_bool_to_text (obj->battery_is_rechargeable));
		if (old->battery_state != obj->battery_state)
			g_print ("    state:               %s -> %s\n",
				 dkp_source_state_to_text (old->battery_state),
				 dkp_source_state_to_text (obj->battery_state));
		if (old->battery_energy != obj->battery_energy)
			g_print ("    energy:              %g -> %g Wh\n",
				 old->battery_energy,
				 obj->battery_energy);
		if (old->battery_energy_empty != obj->battery_energy_empty)
			g_print ("    energy-empty:        %g -> %g Wh\n",
				 old->battery_energy_empty,
				 obj->battery_energy_empty);
		if (old->battery_energy_full != obj->battery_energy_full)
			g_print ("    energy-full:         %g -> %g Wh\n",
				 old->battery_energy_full,
				 obj->battery_energy_full);
		if (old->battery_energy_full_design != obj->battery_energy_full_design)
			g_print ("    energy-full-design:  %g -> %g Wh\n",
				 old->battery_energy_full_design,
				 obj->battery_energy_full_design);
		if (old->battery_energy_rate != obj->battery_energy_rate)
			g_print ("    energy-rate:         %g -> %g W\n",
				 old->battery_energy_rate,
				 obj->battery_energy_rate);

		if (old->battery_time_to_full != obj->battery_time_to_full) {
			time_str_old = dkp_object_time_to_text (old->battery_time_to_full);
			time_str = dkp_object_time_to_text (obj->battery_time_to_full);
			g_print ("    time to full:        %s -> %s\n", time_str_old, time_str);
			g_free (time_str_old);
			g_free (time_str);
		}

		if (old->battery_time_to_empty != obj->battery_time_to_empty) {
			time_str_old = dkp_object_time_to_text (old->battery_time_to_empty);
			time_str = dkp_object_time_to_text (obj->battery_time_to_empty);
			g_print ("    time to empty:       %s -> %s\n", time_str_old, time_str);
			g_free (time_str_old);
			g_free (time_str);
		}

		if (old->battery_percentage != obj->battery_percentage)
			g_print ("    percentage:          %g%% -> %g%%\n",
				 old->battery_percentage,
				 obj->battery_percentage);
		if (old->battery_capacity != obj->battery_capacity)
			g_print ("    capacity:            %g%% -> %g%%\n",
				 old->battery_capacity,
				 obj->battery_capacity);
		if (old->battery_technology != obj->battery_technology)
			g_print ("    technology:          %s -> %s\n",
				 dkp_source_technology_to_text (old->battery_technology),
				 dkp_source_technology_to_text (obj->battery_technology));
	} else if (obj->type == DKP_SOURCE_TYPE_LINE_POWER) {
		g_print ("  line-power\n");
		if (old->line_power_online != obj->line_power_online)
			g_print ("    online:             %s -> %s\n",
				 dkp_object_bool_to_text (old->line_power_online),
				 dkp_object_bool_to_text (obj->line_power_online));
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
 * dkp_object_free_internal:
 **/
static gboolean
dkp_object_free_internal (DkpObject *obj)
{
	g_free (obj->vendor);
	g_free (obj->model);
	g_free (obj->serial);
	g_free (obj->native_path);
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
	dkp_object_free_internal (obj);
	g_free (obj);
	return TRUE;
}

/**
 * dkp_object_clear:
 **/
gboolean
dkp_object_clear (DkpObject *obj)
{
	if (obj == NULL)
		return FALSE;
	dkp_object_free_internal (obj);
	dkp_object_clear_internal (obj);
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

