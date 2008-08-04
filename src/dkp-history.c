/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
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

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <glib/gi18n.h>
#include <gio/gio.h>

#include "dkp-debug.h"
#include "dkp-history.h"
#include "dkp-history-obj.h"

static void	dkp_history_class_init	(DkpHistoryClass	*klass);
static void	dkp_history_init	(DkpHistory		*profile);
static void	dkp_history_finalize	(GObject		*object);

#define DKP_HISTORY_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), DK_TYPE_HISTORY, DkpHistoryPrivate))

#define DKP_HISTORY_DATA_DIR		"/home/hughsie/Desktop"
#define DKP_HISTORY_SAVE_INTERVAL	5 /* seconds */

struct DkpHistoryPrivate
{
	gchar			*id;
	gdouble			 rate_last;
	gdouble			 percentage_last;
	DkpSourceState		 state;
	GPtrArray		*data_rate;
	GPtrArray		*data_charge;
	guint			 save_id;
};

enum {
	DKP_HISTORY_PROGRESS,
	DKP_HISTORY_LAST_SIGNAL
};

G_DEFINE_TYPE (DkpHistory, dkp_history, G_TYPE_OBJECT)
#define DKP_HISTORY_FILE_HEADER	"PackageKit Profile"

/**
 * dkp_history_copy_array_timespan:
 **/
static GPtrArray *
dkp_history_copy_array_timespan (GPtrArray *array, guint timespan)
{
	guint i;
	const DkpHistoryObj *obj;
	DkpHistoryObj *obj_new;
	GPtrArray *array_new;
	guint start;

	/* no data */
	if (array->len == 0)
		return NULL;

	array_new = g_ptr_array_new ();

	/* treat the timespan like a range, and search backwards */
	obj = (const DkpHistoryObj *) g_ptr_array_index (array, array->len-1);
	start = obj->time;
	for (i=array->len-1; i>0; i--) {
		obj = (const DkpHistoryObj *) g_ptr_array_index (array, i);
		if (start - obj->time < timespan) {
			obj_new = dkp_history_obj_copy (obj);
			g_ptr_array_add (array_new, obj_new);
		}
	}

	return array_new;
}

/**
 * dkp_history_get_charge_data:
 **/
GPtrArray *
dkp_history_get_charge_data (DkpHistory *profile, guint timespan)
{
	GPtrArray *array;
	if (profile->priv->id == NULL)
		return NULL;
	array = dkp_history_copy_array_timespan (profile->priv->data_charge, timespan);
	return array;
}

/**
 * dkp_history_get_rate_data:
 **/
GPtrArray *
dkp_history_get_rate_data (DkpHistory *profile, guint timespan)
{
	GPtrArray *array;
	if (profile->priv->id == NULL)
		return NULL;
	array = dkp_history_copy_array_timespan (profile->priv->data_rate, timespan);
	return array;
}

/**
 * dkp_history_load_data:
 **/
static gchar *
dkp_history_get_filename (DkpHistory *profile, const gchar *type)
{
	gchar *path;
	gchar *filename;

	filename = g_strdup_printf ("profile-%s-%s.dat", type, profile->priv->id);
	path = g_build_filename (DKP_HISTORY_DATA_DIR, "DeviceKit-power", filename, NULL);
	g_free (filename);
	return path;
}

/**
 * dkp_history_save_data_array:
 **/
static gboolean
dkp_history_save_data_array (const gchar *filename, GPtrArray *array)
{
	guint i;
	const DkpHistoryObj *obj;
	gchar *part;
	GString *string;
	gboolean ret = TRUE;
	GFile *file = NULL;
	GError *error = NULL;

	/* generate data */
	string = g_string_new ("");
	for (i=0; i<array->len; i++) {
		obj = (const DkpHistoryObj *) g_ptr_array_index (array, i);
		part = dkp_history_obj_to_string (obj);
		if (part == NULL) {
			ret = FALSE;
			break;
		}
		g_string_append_printf (string, "%s\n", part);
		g_free (part);
	}
	part = g_string_free (string, FALSE);

	/* we failed to convert to string */
	if (!ret) {
		dkp_warning ("failed to convert");
		goto out;
	}

	/* save to disk */
	file = g_file_new_for_path (filename);
	ret = g_file_set_contents (filename, part, -1, &error);
//	ret = g_file_replace_contents (file, part, -1, NULL, TRUE, G_FILE_CREATE_NONE, NULL, NULL, &error);
	if (!ret) {
		dkp_warning ("failed to set data: %s", error->message);
		g_error_free (error);
		goto out;
	}
	dkp_debug ("saved %s", filename);

out:
	if (file != NULL)
		g_object_unref (file);
	g_free (part);
	return ret;
}

/**
 * dkp_history_save_data:
 **/
static gboolean
dkp_history_save_data (DkpHistory *profile)
{
	gchar *filename;

	/* load rate history from disk */
	filename = dkp_history_get_filename (profile, "rate");
	dkp_history_save_data_array (filename, profile->priv->data_rate);
	g_free (filename);

	/* load charge history from disk */
	filename = dkp_history_get_filename (profile, "charge");
	dkp_history_save_data_array (filename, profile->priv->data_charge);
	g_free (filename);

	return TRUE;
}

/**
 * dkp_history_schedule_save_cb:
 **/
static gboolean
dkp_history_schedule_save_cb (DkpHistory *profile)
{
	dkp_history_save_data (profile);
	return FALSE;
}

/**
 * dkp_history_is_low_power:
 **/
static gboolean
dkp_history_is_low_power (DkpHistory *profile)
{
	guint length;
	const DkpHistoryObj *obj;

	/* current status is always up to date */
	if (profile->priv->state != DKP_SOURCE_STATE_DISCHARGING)
		return FALSE;

	/* have we got any data? */
	length = profile->priv->data_charge->len;
	if (length == 0)
		return FALSE;

	/* get the last saved charge object */
	obj = (const DkpHistoryObj *) g_ptr_array_index (profile->priv->data_charge, length-1);
	if (obj->state != DKP_SOURCE_STATE_DISCHARGING)
		return FALSE;

	/* high enough */
	if (obj->value > 10)
		return FALSE;

	/* we are low power */
	return TRUE;
}

/**
 * dkp_history_schedule_save:
 **/
static gboolean
dkp_history_schedule_save (DkpHistory *profile)
{
	gboolean ret;

	/* if low power, then don't batch up save requests */
	ret = dkp_history_is_low_power (profile);
	if (ret) {
		dkp_history_save_data (profile);
		return TRUE;
	}

	/* we already have one saved, cancel and reschedule */
	if (profile->priv->save_id != 0) {
		dkp_debug ("deferring as others queued");
		g_source_remove (profile->priv->save_id);
		profile->priv->save_id = g_timeout_add_seconds (DKP_HISTORY_SAVE_INTERVAL,
								(GSourceFunc) dkp_history_schedule_save_cb, profile);
		return TRUE;
	}

	/* nothing scheduled, do new */
	dkp_debug ("saving in %i seconds", DKP_HISTORY_SAVE_INTERVAL);
	profile->priv->save_id = g_timeout_add_seconds (DKP_HISTORY_SAVE_INTERVAL,
							(GSourceFunc) dkp_history_schedule_save_cb, profile);

	return TRUE;
}

/**
 * dkp_history_load_data_array:
 **/
static gboolean
dkp_history_load_data_array (const gchar *filename, GPtrArray *array)
{
	gboolean ret;
	GFile *file = NULL;
	GError *error = NULL;
	gchar *data = NULL;
	gchar **parts = NULL;
	guint i;
	guint length;
	DkpHistoryObj *obj;

	/* do we exist */
	ret = g_file_test (filename, G_FILE_TEST_EXISTS);
	if (!ret) {
		dkp_debug ("failed to get data from %s as file does not exist", filename);
		goto out;
	}

	/* get contents */
	file = g_file_new_for_path (filename);
	ret = g_file_load_contents (file, NULL, &data, NULL, NULL, &error);
	if (!ret) {
		dkp_warning ("failed to get data: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* split by line ending */
	parts = g_strsplit (data, "\n", 0);
	length = g_strv_length (parts) - 1;
	dkp_debug ("loading %i items of data from %s", length, filename);
	for (i=0; i<length; i++) {
		obj = dkp_history_obj_from_string (parts[i]);
		if (obj != NULL)
			g_ptr_array_add (array, obj);
	}

out:
	g_strfreev (parts);
	g_free (data);
	if (file != NULL)
		g_object_unref (file);

	return ret;
}

/**
 * dkp_history_load_data:
 **/
static gboolean
dkp_history_load_data (DkpHistory *profile)
{
	gchar *filename;

	/* load rate history from disk */
	filename = dkp_history_get_filename (profile, "rate");
	dkp_history_load_data_array (filename, profile->priv->data_rate);
	g_free (filename);

	/* load charge history from disk */
	filename = dkp_history_get_filename (profile, "charge");
	dkp_history_load_data_array (filename, profile->priv->data_charge);
	g_free (filename);

	return TRUE;
}

/**
 * dkp_history_set_id:
 **/
gboolean
dkp_history_set_id (DkpHistory *profile, const gchar *id)
{
	gboolean ret;

	if (profile->priv->id != NULL)
		return FALSE;
	if (id == NULL)
		return FALSE;

	dkp_debug ("using id: %s", id);
	profile->priv->id = g_strdup (id);
	/* load all previous data */
	ret = dkp_history_load_data (profile);
	return ret;
}

/**
 * dkp_history_set_state:
 **/
gboolean
dkp_history_set_state (DkpHistory *profile, DkpSourceState state)
{
	if (profile->priv->id == NULL)
		return FALSE;
	profile->priv->state = state;
	return TRUE;
}

/**
 * dkp_history_set_charge_data:
 **/
gboolean
dkp_history_set_charge_data (DkpHistory *profile, gdouble percentage)
{
	DkpHistoryObj *obj;

	if (profile->priv->id == NULL)
		return FALSE;
	if (profile->priv->state == DKP_SOURCE_STATE_UNKNOWN)
		return FALSE;
	if (profile->priv->percentage_last == percentage)
		return FALSE;

	/* add to array and schedule save file */
	obj = dkp_history_obj_create (percentage, profile->priv->state);
	g_ptr_array_add (profile->priv->data_charge, obj);
	dkp_history_schedule_save (profile);

	/* save last value */
	profile->priv->percentage_last = percentage;

	return TRUE;
}

/**
 * dkp_history_set_rate_data:
 **/
gboolean
dkp_history_set_rate_data (DkpHistory *profile, gdouble rate)
{
	DkpHistoryObj *obj;

	if (profile->priv->id == NULL)
		return FALSE;
	if (profile->priv->state == DKP_SOURCE_STATE_UNKNOWN)
		return FALSE;
	if (profile->priv->rate_last == rate)
		return FALSE;

	/* add to array and schedule save file */
	obj = dkp_history_obj_create (rate, profile->priv->state);
	g_ptr_array_add (profile->priv->data_rate, obj);
	dkp_history_schedule_save (profile);

	/* save last value */
	profile->priv->rate_last = rate;

	return TRUE;
}

/**
 * dkp_history_class_init:
 * @klass: The DkpHistoryClass
 **/
static void
dkp_history_class_init (DkpHistoryClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = dkp_history_finalize;
	g_type_class_add_private (klass, sizeof (DkpHistoryPrivate));
}

/**
 * dkp_history_init:
 * @profile: This class instance
 **/
static void
dkp_history_init (DkpHistory *profile)
{
	profile->priv = DKP_HISTORY_GET_PRIVATE (profile);
	profile->priv->id = NULL;
	profile->priv->rate_last = 0;
	profile->priv->percentage_last = 0;
	profile->priv->state = DKP_SOURCE_STATE_UNKNOWN;
	profile->priv->data_rate = g_ptr_array_new ();
	profile->priv->data_charge = g_ptr_array_new ();
	profile->priv->save_id = 0;
}

/**
 * dkp_history_finalize:
 * @object: The object to finalize
 **/
static void
dkp_history_finalize (GObject *object)
{
	DkpHistory *profile;

	g_return_if_fail (DK_IS_HISTORY (object));

	profile = DKP_HISTORY (object);

	/* save */
	dkp_history_save_data (profile);

	g_ptr_array_free (profile->priv->data_rate, TRUE);
	g_ptr_array_free (profile->priv->data_charge, TRUE);
	g_free (profile->priv->id);

	g_return_if_fail (profile->priv != NULL);

	G_OBJECT_CLASS (dkp_history_parent_class)->finalize (object);
}

/**
 * dkp_history_new:
 *
 * Return value: a new DkpHistory object.
 **/
DkpHistory *
dkp_history_new (void)
{
	DkpHistory *profile;
	profile = g_object_new (DK_TYPE_HISTORY, NULL);
	return DKP_HISTORY (profile);
}

