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

#include "egg-debug.h"
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
	EggObjList		*data_rate;
	EggObjList		*data_charge;
	EggObjList		*data_time_full;
	EggObjList		*data_time_empty;
	guint			 save_id;
};

enum {
	DKP_HISTORY_PROGRESS,
	DKP_HISTORY_LAST_SIGNAL
};

G_DEFINE_TYPE (DkpHistory, dkp_history, G_TYPE_OBJECT)
#define DKP_HISTORY_FILE_HEADER	"PackageKit Profile"

/**
 * dkp_history_get_history_list:
 **/
static EggObjList *
dkp_history_new_history_list (void)
{
	EggObjList *list;
	list = egg_obj_list_new ();
	egg_obj_list_set_new (list, (EggObjListNewFunc) dkp_history_obj_new);
	egg_obj_list_set_copy (list, (EggObjListCopyFunc) dkp_history_obj_copy);
	egg_obj_list_set_free (list, (EggObjListFreeFunc) dkp_history_obj_free);
	egg_obj_list_set_to_string (list, (EggObjListToStringFunc) dkp_history_obj_to_string);
	egg_obj_list_set_from_string (list, (EggObjListFromStringFunc) dkp_history_obj_from_string);
	return list;
}

/**
 * dkp_history_copy_array_timespan:
 **/
static EggObjList *
dkp_history_copy_array_timespan (EggObjList *array, guint timespan)
{
	guint i;
	const DkpHistoryObj *obj;
	EggObjList *array_new;
	guint start;

	/* no data */
	if (array->len == 0)
		return NULL;

	/* new data */
	array_new = dkp_history_new_history_list ();

	/* treat the timespan like a range, and search backwards */
	obj = (const DkpHistoryObj *) egg_obj_list_index (array, array->len-1);
	start = obj->time;
	for (i=array->len-1; i>0; i--) {
		obj = (const DkpHistoryObj *) egg_obj_list_index (array, i);
		if (start - obj->time < timespan)
			egg_obj_list_add (array_new, (const gpointer) obj);
	}

	return array_new;
}

/**
 * dkp_history_get_charge_data:
 **/
EggObjList *
dkp_history_get_charge_data (DkpHistory *history, guint timespan)
{
	EggObjList *array;

	g_return_val_if_fail (DKP_IS_HISTORY (history), NULL);

	if (history->priv->id == NULL)
		return NULL;
	array = dkp_history_copy_array_timespan (history->priv->data_charge, timespan);
	return array;
}

/**
 * dkp_history_get_rate_data:
 **/
EggObjList *
dkp_history_get_rate_data (DkpHistory *history, guint timespan)
{
	EggObjList *array;

	g_return_val_if_fail (DKP_IS_HISTORY (history), NULL);

	if (history->priv->id == NULL)
		return NULL;
	array = dkp_history_copy_array_timespan (history->priv->data_rate, timespan);
	return array;
}

/**
 * dkp_history_get_time_full_data:
 **/
EggObjList *
dkp_history_get_time_full_data (DkpHistory *history, guint timespan)
{
	EggObjList *array;

	g_return_val_if_fail (DKP_IS_HISTORY (history), NULL);

	if (history->priv->id == NULL)
		return NULL;
	array = dkp_history_copy_array_timespan (history->priv->data_time_full, timespan);
	return array;
}

/**
 * dkp_history_get_time_empty_data:
 **/
EggObjList *
dkp_history_get_time_empty_data (DkpHistory *history, guint timespan)
{
	EggObjList *array;

	g_return_val_if_fail (DKP_IS_HISTORY (history), NULL);

	if (history->priv->id == NULL)
		return NULL;
	array = dkp_history_copy_array_timespan (history->priv->data_time_empty, timespan);
	return array;
}

/**
 * dkp_history_get_filename:
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
 * dkp_history_save_data:
 **/
static gboolean
dkp_history_save_data (DkpHistory *history)
{
	gchar *filename;

	/* we have an ID? */
	if (history->priv->id == NULL) {
		egg_warning ("no ID, cannot save");
		return FALSE;
	}

	/* save rate history to disk */
	filename = dkp_history_get_filename (history, "rate");
	egg_obj_list_to_file (history->priv->data_rate, filename);
	g_free (filename);

	/* save charge history to disk */
	filename = dkp_history_get_filename (history, "charge");
	egg_obj_list_to_file (history->priv->data_charge, filename);
	g_free (filename);

	/* save charge history to disk */
	filename = dkp_history_get_filename (history, "time-full");
	egg_obj_list_to_file (history->priv->data_time_full, filename);
	g_free (filename);

	/* save charge history to disk */
	filename = dkp_history_get_filename (history, "time-empty");
	egg_obj_list_to_file (history->priv->data_time_empty, filename);
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
	obj = (const DkpHistoryObj *) egg_obj_list_index (history->priv->data_charge, length-1);
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
		egg_debug ("deferring as others queued");
		g_source_remove (history->priv->save_id);
		history->priv->save_id = g_timeout_add_seconds (DKP_HISTORY_SAVE_INTERVAL,
								(GSourceFunc) dkp_history_schedule_save_cb, history);
		return TRUE;
	}

	/* nothing scheduled, do new */
	egg_debug ("saving in %i seconds", DKP_HISTORY_SAVE_INTERVAL);
	history->priv->save_id = g_timeout_add_seconds (DKP_HISTORY_SAVE_INTERVAL,
							(GSourceFunc) dkp_history_schedule_save_cb, history);

	return TRUE;
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
	egg_obj_list_from_file (history->priv->data_rate, filename);
	g_free (filename);

	/* load charge history from disk */
	filename = dkp_history_get_filename (history, "charge");
	egg_obj_list_from_file (history->priv->data_charge, filename);
	g_free (filename);

	/* load charge history from disk */
	filename = dkp_history_get_filename (history, "time-full");
	egg_obj_list_from_file (history->priv->data_time_full, filename);
	g_free (filename);

	/* load charge history from disk */
	filename = dkp_history_get_filename (history, "time-empty");
	egg_obj_list_from_file (history->priv->data_time_empty, filename);
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

	egg_debug ("using id: %s", id);
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
	egg_obj_list_add (history->priv->data_charge, obj);
	dkp_history_obj_free (obj);
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
	egg_obj_list_add (history->priv->data_rate, obj);
	dkp_history_obj_free (obj);
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
	egg_obj_list_add (history->priv->data_time_full, obj);
	dkp_history_obj_free (obj);
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
	egg_obj_list_add (history->priv->data_time_empty, obj);
	dkp_history_obj_free (obj);
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
	history->priv->data_rate = dkp_history_new_history_list ();
	history->priv->data_charge = dkp_history_new_history_list ();
	history->priv->data_time_full = dkp_history_new_history_list ();
	history->priv->data_time_empty = dkp_history_new_history_list ();
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

	g_object_unref (history->priv->data_rate);
	g_object_unref (history->priv->data_charge);
	g_object_unref (history->priv->data_time_full);
	g_object_unref (history->priv->data_time_empty);
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

