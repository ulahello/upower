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
static void	dkp_history_init	(DkpHistory		*history);
static void	dkp_history_finalize	(GObject		*object);

#define DKP_HISTORY_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), DKP_TYPE_HISTORY, DkpHistoryPrivate))

#define DKP_HISTORY_SAVE_INTERVAL	5 /* seconds */

struct DkpHistoryPrivate
{
	gchar			*id;
	gdouble			 rate_last;
	gint64			 time_full_last;
	gint64			 time_empty_last;
	gdouble			 percentage_last;
	DkpDeviceState		 state;
	GPtrArray		*data_rate;
	GPtrArray		*data_charge;
	GPtrArray		*data_time_full;
	GPtrArray		*data_time_empty;
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
dkp_history_get_charge_data (DkpHistory *history, guint timespan)
{
	GPtrArray *array;

	g_return_val_if_fail (DKP_IS_HISTORY (history), NULL);

	if (history->priv->id == NULL)
		return NULL;
	array = dkp_history_copy_array_timespan (history->priv->data_charge, timespan);
	return array;
}

/**
 * dkp_history_get_rate_data:
 **/
GPtrArray *
dkp_history_get_rate_data (DkpHistory *history, guint timespan)
{
	GPtrArray *array;

	g_return_val_if_fail (DKP_IS_HISTORY (history), NULL);

	if (history->priv->id == NULL)
		return NULL;
	array = dkp_history_copy_array_timespan (history->priv->data_rate, timespan);
	return array;
}

/**
 * dkp_history_get_time_full_data:
 **/
GPtrArray *
dkp_history_get_time_full_data (DkpHistory *history, guint timespan)
{
	GPtrArray *array;

	g_return_val_if_fail (DKP_IS_HISTORY (history), NULL);

	if (history->priv->id == NULL)
		return NULL;
	array = dkp_history_copy_array_timespan (history->priv->data_time_full, timespan);
	return array;
}

/**
 * dkp_history_get_time_empty_data:
 **/
GPtrArray *
dkp_history_get_time_empty_data (DkpHistory *history, guint timespan)
{
	GPtrArray *array;

	g_return_val_if_fail (DKP_IS_HISTORY (history), NULL);

	if (history->priv->id == NULL)
		return NULL;
	array = dkp_history_copy_array_timespan (history->priv->data_time_empty, timespan);
	return array;
}

/**
 * dkp_history_load_data:
 **/
static gchar *
dkp_history_get_filename (DkpHistory *history, const gchar *type)
{
	gchar *path;
	gchar *filename;

	filename = g_strdup_printf ("history-%s-%s.dat", type, history->priv->id);
	path = g_build_filename (PACKAGE_LOCALSTATE_DIR, "lib", "DeviceKit-power", filename, NULL);
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
dkp_history_save_data (DkpHistory *history)
{
	gchar *filename;

	/* we have an ID? */
	if (history->priv->id == NULL) {
		dkp_warning ("no ID, cannot save");
		return FALSE;
	}

	/* save rate history to disk */
	filename = dkp_history_get_filename (history, "rate");
	dkp_history_save_data_array (filename, history->priv->data_rate);
	g_free (filename);

	/* save charge history to disk */
	filename = dkp_history_get_filename (history, "charge");
	dkp_history_save_data_array (filename, history->priv->data_charge);
	g_free (filename);

	/* save charge history to disk */
	filename = dkp_history_get_filename (history, "time-full");
	dkp_history_save_data_array (filename, history->priv->data_time_full);
	g_free (filename);

	/* save charge history to disk */
	filename = dkp_history_get_filename (history, "time-empty");
	dkp_history_save_data_array (filename, history->priv->data_time_empty);
	g_free (filename);

	return TRUE;
}

/**
 * dkp_history_schedule_save_cb:
 **/
static gboolean
dkp_history_schedule_save_cb (DkpHistory *history)
{
	dkp_history_save_data (history);
	return FALSE;
}

/**
 * dkp_history_is_low_power:
 **/
static gboolean
dkp_history_is_low_power (DkpHistory *history)
{
	guint length;
	const DkpHistoryObj *obj;

	/* current status is always up to date */
	if (history->priv->state != DKP_DEVICE_STATE_DISCHARGING)
		return FALSE;

	/* have we got any data? */
	length = history->priv->data_charge->len;
	if (length == 0)
		return FALSE;

	/* get the last saved charge object */
	obj = (const DkpHistoryObj *) g_ptr_array_index (history->priv->data_charge, length-1);
	if (obj->state != DKP_DEVICE_STATE_DISCHARGING)
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
dkp_history_schedule_save (DkpHistory *history)
{
	gboolean ret;

	/* if low power, then don't batch up save requests */
	ret = dkp_history_is_low_power (history);
	if (ret) {
		dkp_history_save_data (history);
		return TRUE;
	}

	/* we already have one saved, cancel and reschedule */
	if (history->priv->save_id != 0) {
		dkp_debug ("deferring as others queued");
		g_source_remove (history->priv->save_id);
		history->priv->save_id = g_timeout_add_seconds (DKP_HISTORY_SAVE_INTERVAL,
								(GSourceFunc) dkp_history_schedule_save_cb, history);
		return TRUE;
	}

	/* nothing scheduled, do new */
	dkp_debug ("saving in %i seconds", DKP_HISTORY_SAVE_INTERVAL);
	history->priv->save_id = g_timeout_add_seconds (DKP_HISTORY_SAVE_INTERVAL,
							(GSourceFunc) dkp_history_schedule_save_cb, history);

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
	length = g_strv_length (parts);
	if (length == 0) {
		dkp_debug ("no data in %s", filename);
		goto out;
	}

	/* add valid entries */
	dkp_debug ("loading %i items of data from %s", length, filename);
	for (i=0; i<length-1; i++) {
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
dkp_history_load_data (DkpHistory *history)
{
	gchar *filename;

	/* load rate history from disk */
	filename = dkp_history_get_filename (history, "rate");
	dkp_history_load_data_array (filename, history->priv->data_rate);
	g_free (filename);

	/* load charge history from disk */
	filename = dkp_history_get_filename (history, "charge");
	dkp_history_load_data_array (filename, history->priv->data_charge);
	g_free (filename);

	/* load charge history from disk */
	filename = dkp_history_get_filename (history, "time-full");
	dkp_history_load_data_array (filename, history->priv->data_time_full);
	g_free (filename);

	/* load charge history from disk */
	filename = dkp_history_get_filename (history, "time-empty");
	dkp_history_load_data_array (filename, history->priv->data_time_empty);
	g_free (filename);

	return TRUE;
}

/**
 * dkp_history_set_id:
 **/
gboolean
dkp_history_set_id (DkpHistory *history, const gchar *id)
{
	gboolean ret;

	g_return_val_if_fail (DKP_IS_HISTORY (history), FALSE);

	if (history->priv->id != NULL)
		return FALSE;
	if (id == NULL)
		return FALSE;

	dkp_debug ("using id: %s", id);
	history->priv->id = g_strdup (id);
	/* load all previous data */
	ret = dkp_history_load_data (history);
	return ret;
}

/**
 * dkp_history_set_state:
 **/
gboolean
dkp_history_set_state (DkpHistory *history, DkpDeviceState state)
{
	g_return_val_if_fail (DKP_IS_HISTORY (history), FALSE);

	if (history->priv->id == NULL)
		return FALSE;
	history->priv->state = state;
	return TRUE;
}

/**
 * dkp_history_set_charge_data:
 **/
gboolean
dkp_history_set_charge_data (DkpHistory *history, gdouble percentage)
{
	DkpHistoryObj *obj;

	g_return_val_if_fail (DKP_IS_HISTORY (history), FALSE);

	if (history->priv->id == NULL)
		return FALSE;
	if (history->priv->state == DKP_DEVICE_STATE_UNKNOWN)
		return FALSE;
	if (history->priv->percentage_last == percentage)
		return FALSE;

	/* add to array and schedule save file */
	obj = dkp_history_obj_create (percentage, history->priv->state);
	g_ptr_array_add (history->priv->data_charge, obj);
	dkp_history_schedule_save (history);

	/* save last value */
	history->priv->percentage_last = percentage;

	return TRUE;
}

/**
 * dkp_history_set_rate_data:
 **/
gboolean
dkp_history_set_rate_data (DkpHistory *history, gdouble rate)
{
	DkpHistoryObj *obj;

	g_return_val_if_fail (DKP_IS_HISTORY (history), FALSE);

	if (history->priv->id == NULL)
		return FALSE;
	if (history->priv->state == DKP_DEVICE_STATE_UNKNOWN)
		return FALSE;
	if (history->priv->rate_last == rate)
		return FALSE;

	/* add to array and schedule save file */
	obj = dkp_history_obj_create (rate, history->priv->state);
	g_ptr_array_add (history->priv->data_rate, obj);
	dkp_history_schedule_save (history);

	/* save last value */
	history->priv->rate_last = rate;

	return TRUE;
}

/**
 * dkp_history_set_time_full_data:
 **/
gboolean
dkp_history_set_time_full_data (DkpHistory *history, gint64 time)
{
	DkpHistoryObj *obj;

	g_return_val_if_fail (DKP_IS_HISTORY (history), FALSE);

	if (history->priv->id == NULL)
		return FALSE;
	if (history->priv->state == DKP_DEVICE_STATE_UNKNOWN)
		return FALSE;
	if (time < 0)
		return FALSE;
	if (history->priv->time_full_last == time)
		return FALSE;

	/* add to array and schedule save file */
	obj = dkp_history_obj_create ((gdouble) time, history->priv->state);
	g_ptr_array_add (history->priv->data_time_full, obj);
	dkp_history_schedule_save (history);

	/* save last value */
	history->priv->time_full_last = time;

	return TRUE;
}

/**
 * dkp_history_set_time_empty_data:
 **/
gboolean
dkp_history_set_time_empty_data (DkpHistory *history, gint64 time)
{
	DkpHistoryObj *obj;

	g_return_val_if_fail (DKP_IS_HISTORY (history), FALSE);

	if (history->priv->id == NULL)
		return FALSE;
	if (history->priv->state == DKP_DEVICE_STATE_UNKNOWN)
		return FALSE;
	if (time < 0)
		return FALSE;
	if (history->priv->time_empty_last == time)
		return FALSE;

	/* add to array and schedule save file */
	obj = dkp_history_obj_create ((gdouble) time, history->priv->state);
	g_ptr_array_add (history->priv->data_time_empty, obj);
	dkp_history_schedule_save (history);

	/* save last value */
	history->priv->time_empty_last = time;

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
 * @history: This class instance
 **/
static void
dkp_history_init (DkpHistory *history)
{
	history->priv = DKP_HISTORY_GET_PRIVATE (history);
	history->priv->id = NULL;
	history->priv->rate_last = 0;
	history->priv->percentage_last = 0;
	history->priv->state = DKP_DEVICE_STATE_UNKNOWN;
	history->priv->data_rate = g_ptr_array_new ();
	history->priv->data_charge = g_ptr_array_new ();
	history->priv->data_time_full = g_ptr_array_new ();
	history->priv->data_time_empty = g_ptr_array_new ();
	history->priv->save_id = 0;
}

/**
 * dkp_history_finalize:
 * @object: The object to finalize
 **/
static void
dkp_history_finalize (GObject *object)
{
	DkpHistory *history;

	g_return_if_fail (DKP_IS_HISTORY (object));

	history = DKP_HISTORY (object);

	/* save */
	dkp_history_save_data (history);

	g_ptr_array_free (history->priv->data_rate, TRUE);
	g_ptr_array_free (history->priv->data_charge, TRUE);
	g_ptr_array_free (history->priv->data_time_full, TRUE);
	g_ptr_array_free (history->priv->data_time_empty, TRUE);
	g_free (history->priv->id);

	g_return_if_fail (history->priv != NULL);

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
	DkpHistory *history;
	history = g_object_new (DKP_TYPE_HISTORY, NULL);
	return DKP_HISTORY (history);
}

