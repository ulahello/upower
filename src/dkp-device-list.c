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
#include <glib.h>

#include "egg-debug.h"
#include "dkp-device.h"
#include "dkp-device-list.h"

static void	dkp_device_list_class_init	(DkpDeviceListClass	*klass);
static void	dkp_device_list_init		(DkpDeviceList		*list);
static void	dkp_device_list_finalize	(GObject		*object);

#define DKP_DEVICE_LIST_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), DKP_TYPE_DEVICE_LIST, DkpDeviceListPrivate))

struct DkpDeviceListPrivate
{
	GPtrArray		*array;
	GHashTable		*map_native_path_to_device;
};

G_DEFINE_TYPE (DkpDeviceList, dkp_device_list, G_TYPE_OBJECT)

/**
 * dkp_device_list_lookup:
 *
 * Convert a %GUdevDevice into a %DkpDevice -- we use the native path
 * to look these up as it's the only thing they share.
 **/
DkpDevice *
dkp_device_list_lookup (DkpDeviceList *list, GUdevDevice *d)
{
	DkpDevice *device;
	const gchar *native_path;

	g_return_val_if_fail (DKP_IS_DEVICE_LIST (list), NULL);

	/* does device exist in db? */
	native_path = g_udev_device_get_sysfs_path (d);
	device = g_hash_table_lookup (list->priv->map_native_path_to_device, native_path);
	return device;
}

/**
 * dkp_device_list_insert:
 *
 * Insert a %GUdevDevice device and it's mapping to a %DkpDevice device
 * into a list of devices.
 **/
gboolean
dkp_device_list_insert (DkpDeviceList *list, GUdevDevice *d, DkpDevice *device)
{
	const gchar *native_path;

	g_return_val_if_fail (DKP_IS_DEVICE_LIST (list), FALSE);
	g_return_val_if_fail (d != NULL, FALSE);
	g_return_val_if_fail (device != NULL, FALSE);

	native_path = g_udev_device_get_sysfs_path (d);
	g_hash_table_insert (list->priv->map_native_path_to_device,
			     g_strdup (native_path), device);
	g_ptr_array_add (list->priv->array, device);
	egg_debug ("added %s", native_path);
	return TRUE;
}

/**
 * dkp_device_list_remove_cb:
 **/
static gboolean
dkp_device_list_remove_cb (gpointer key, gpointer value, gpointer user_data)
{
	if (value == user_data) {
		egg_debug ("removed %s", (char *) key);
		return TRUE;
	}
	return FALSE;
}

/**
 * dkp_device_list_remove:
 **/
gboolean
dkp_device_list_remove (DkpDeviceList *list, DkpDevice *device)
{
	g_return_val_if_fail (DKP_IS_DEVICE_LIST (list), FALSE);
	g_return_val_if_fail (device != NULL, FALSE);

	/* remove the device from the db */
	g_hash_table_foreach_remove (list->priv->map_native_path_to_device,
				     dkp_device_list_remove_cb, device);
	g_ptr_array_remove (list->priv->array, device);
	return TRUE;
}

/**
 * dkp_device_list_get_array:
 *
 * This is quick to iterate when we don't have GUdevDevice's to resolve
 **/
const GPtrArray	*
dkp_device_list_get_array (DkpDeviceList *list)
{
	g_return_val_if_fail (DKP_IS_DEVICE_LIST (list), NULL);
	return list->priv->array;
}

/**
 * dkp_device_list_class_init:
 * @klass: The DkpDeviceListClass
 **/
static void
dkp_device_list_class_init (DkpDeviceListClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = dkp_device_list_finalize;
	g_type_class_add_private (klass, sizeof (DkpDeviceListPrivate));
}

/**
 * dkp_device_list_init:
 * @list: This class instance
 **/
static void
dkp_device_list_init (DkpDeviceList *list)
{
	list->priv = DKP_DEVICE_LIST_GET_PRIVATE (list);
	list->priv->array = g_ptr_array_new ();
	list->priv->map_native_path_to_device = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
}

/**
 * dkp_device_list_finalize:
 * @object: The object to finalize
 **/
static void
dkp_device_list_finalize (GObject *object)
{
	DkpDeviceList *list;

	g_return_if_fail (DKP_IS_DEVICE_LIST (object));

	list = DKP_DEVICE_LIST (object);
	g_ptr_array_free (list->priv->array, TRUE);
	g_hash_table_unref (list->priv->map_native_path_to_device);

	G_OBJECT_CLASS (dkp_device_list_parent_class)->finalize (object);
}

/**
 * dkp_device_list_new:
 *
 * Return value: a new DkpDeviceList object.
 **/
DkpDeviceList *
dkp_device_list_new (void)
{
	DkpDeviceList *list;
	list = g_object_new (DKP_TYPE_DEVICE_LIST, NULL);
	return DKP_DEVICE_LIST (list);
}

