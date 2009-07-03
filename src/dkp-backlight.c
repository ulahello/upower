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

#include <glib.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <glib/gi18n.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <devkit-gobject/devkit-gobject.h>

#include "egg-debug.h"
#include "sysfs-utils.h"

#include "dkp-backlight.h"
#include "dkp-daemon.h"
#include "dkp-backlight-glue.h"

static void     dkp_backlight_class_init (DkpBacklightClass *klass);
static void     dkp_backlight_init       (DkpBacklight      *backlight);
static void     dkp_backlight_finalize   (GObject	*object);

#define DKP_BACKLIGHT_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), DKP_TYPE_BACKLIGHT, DkpBacklightPrivate))

struct DkpBacklightPrivate
{
	DevkitDevice		*device;
	DBusGConnection		*connection;
	gboolean		 action_in_hardware;
	guint			 actual;
	guint			 maximum;
};

enum
{
	BRIGHTNESS_CHANGED,
	LAST_SIGNAL
};

enum
{
	PROP_0,
	PROP_ACTUAL,
	PROP_MAXIMUM,
	PROP_ACTION_IN_HARDWARE,
	PROP_LAST
};

static guint signals [LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (DkpBacklight, dkp_backlight, G_TYPE_OBJECT)

/**
 * dkp_backlight_refresh_internal:
 **/
static gboolean
dkp_backlight_refresh_internal (DkpBacklight *backlight)
{
	const gchar *native_path;

	g_return_val_if_fail (DKP_IS_BACKLIGHT (backlight), FALSE);

	/* get new value */
	native_path = devkit_device_get_native_path (backlight->priv->device);
	backlight->priv->actual = sysfs_get_int (native_path, "actual_brightness");

	return TRUE;
}

/**
 * dkp_backlight_changed:
 **/
gboolean
dkp_backlight_changed (DkpBacklight *backlight)
{
	const gchar *native_path;

	g_return_val_if_fail (DKP_IS_BACKLIGHT (backlight), FALSE);

	/* get new value */
	native_path = devkit_device_get_native_path (backlight->priv->device);
	backlight->priv->actual = sysfs_get_int (native_path, "actual_brightness");

	egg_debug ("backlight device changed %s (%i/%i)", native_path, backlight->priv->actual, backlight->priv->maximum);

	/* emit */
	g_signal_emit (backlight, signals [BRIGHTNESS_CHANGED], 0, backlight->priv->actual);

	return TRUE;
}

/**
 * dkp_backlight_set_device:
 **/
gboolean
dkp_backlight_set_device (DkpBacklight *backlight, DevkitDevice *device)
{
	const gchar *native_path;
	const gchar *value;

	g_return_val_if_fail (DKP_IS_BACKLIGHT (backlight), FALSE);

	backlight->priv->device = g_object_ref (device);

	/* coldplug */
	native_path = devkit_device_get_native_path (device);
	backlight->priv->actual = sysfs_get_int (native_path, "actual_brightness");
	backlight->priv->maximum = sysfs_get_int (native_path, "max_brightness");

	/* EC does it's own updates */
	value = devkit_device_get_property (device, "DKP_ACTION_IN_HARDWARE");
	if (value != NULL)
		backlight->priv->action_in_hardware = TRUE;

	egg_debug ("adding backlight device %s (%i/%i)", native_path, backlight->priv->actual, backlight->priv->maximum);

	return TRUE;
}

/**
 * dkp_backlight_set_brightness:
 **/
void
dkp_backlight_set_brightness (DkpBacklight *backlight, guint value, DBusGMethodInvocation *context)
{
	GError *error;
	gchar *value_text = NULL;
	const gchar *native_path;
	gchar *path = NULL;
	gint fd = -1;
	ssize_t wrote;
	gint len;

	if (value > backlight->priv->maximum) {
		error = g_error_new (DKP_DAEMON_ERROR, DKP_DAEMON_ERROR_GENERAL, "value too high: %i > %i", value, backlight->priv->maximum);
		dbus_g_method_return_error (context, error);
		goto out;
	}


	/* TODO: use GIO and async write */
	native_path = devkit_device_get_native_path (backlight->priv->device);
	path = g_build_filename (native_path, "brightness", NULL);

	/* open file */
	fd = open (path, O_WRONLY);
	if (fd < 0) {
		error = g_error_new (DKP_DAEMON_ERROR, DKP_DAEMON_ERROR_GENERAL, "failed to open file");
		dbus_g_method_return_error (context, error);
		goto out;
	}

	/* write chunk */
	value_text = g_strdup_printf ("%u", value);
	len = strlen (value_text);
	egg_debug ("Writing '%s' to '%s'", value_text, path);
	wrote = write (fd, value_text, len);
	if (wrote < len) {
		error = g_error_new (DKP_DAEMON_ERROR, DKP_DAEMON_ERROR_GENERAL, "failed to write %i/%i", wrote, len);
		dbus_g_method_return_error (context, error);
		goto out;
	}

	/* emit */
	g_signal_emit (backlight, signals [BRIGHTNESS_CHANGED], 0, value);

	/* success */
	dbus_g_method_return (context, NULL);
out:
	if (fd >= 0)
		close (fd);
	g_free (value_text);
	g_free (path);
}

/**
 * dkp_backlight_get_property:
 **/
static void
dkp_backlight_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	DkpBacklight *backlight = DKP_BACKLIGHT (object);
	switch (prop_id) {
	case PROP_ACTUAL:
		/* FIXME: for now do a hack to read the new value from sysfs
		 * as backlight devices do not issue uevents when they change */
		dkp_backlight_refresh_internal (backlight);
		g_value_set_uint (value, backlight->priv->actual);
		break;
	case PROP_MAXIMUM:
		g_value_set_uint (value, backlight->priv->maximum);
		break;
	case PROP_ACTION_IN_HARDWARE:
		g_value_set_boolean (value, backlight->priv->action_in_hardware);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * dkp_backlight_set_property:
 **/
static void
dkp_backlight_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	DkpBacklight *backlight = DKP_BACKLIGHT (object);

	switch (prop_id) {
	case PROP_ACTUAL:
		backlight->priv->actual = g_value_get_uint (value);
		break;
	case PROP_MAXIMUM:
		backlight->priv->maximum = g_value_get_uint (value);
		break;
	case PROP_ACTION_IN_HARDWARE:
		backlight->priv->action_in_hardware = g_value_get_boolean (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * dkp_backlight_class_init:
 **/
static void
dkp_backlight_class_init (DkpBacklightClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = dkp_backlight_finalize;
	object_class->get_property = dkp_backlight_get_property;
	object_class->set_property = dkp_backlight_set_property;

	signals [BRIGHTNESS_CHANGED] =
		g_signal_new ("brightness-changed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (DkpBacklightClass, brightness_changed),
			      NULL, NULL, g_cclosure_marshal_VOID__UINT,
			      G_TYPE_NONE, 1, G_TYPE_UINT);

	/**
	 * DkpBacklight:actual:
	 */
	g_object_class_install_property (object_class,
					 PROP_ACTUAL,
					 g_param_spec_uint ("actual",
							    NULL, NULL,
							    0,
							    G_MAXUINT,
							    0,
							    G_PARAM_READWRITE));

	/**
	 * DkpBacklight:maximum:
	 */
	g_object_class_install_property (object_class,
					 PROP_MAXIMUM,
					 g_param_spec_uint ("maximum",
							    NULL, NULL,
							    0,
							    G_MAXUINT,
							    0,
							    G_PARAM_READWRITE));
	/**
	 * DkpBacklight:action-in-hardware:
	 */
	g_object_class_install_property (object_class,
					 PROP_ACTION_IN_HARDWARE,
					 g_param_spec_boolean ("action-in-hardware",
							       NULL, NULL,
							       FALSE,
							       G_PARAM_READWRITE));

	/* introspection */
	dbus_g_object_type_install_info (DKP_TYPE_BACKLIGHT, &dbus_glib_dkp_backlight_object_info);

	g_type_class_add_private (klass, sizeof (DkpBacklightPrivate));
}

/**
 * dkp_backlight_init:
 **/
static void
dkp_backlight_init (DkpBacklight *backlight)
{
	GError *error = NULL;

	backlight->priv = DKP_BACKLIGHT_GET_PRIVATE (backlight);

	backlight->priv->device = NULL;
	backlight->priv->actual = 0;
	backlight->priv->maximum = 0;
	backlight->priv->action_in_hardware = FALSE;

	backlight->priv->connection = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
	if (error != NULL) {
		egg_warning ("Cannot connect to bus: %s", error->message);
		g_error_free (error);
		return;
	}

	/* register on the bus */
	dbus_g_connection_register_g_object (backlight->priv->connection, "/org/freedesktop/DeviceKit/Power/Backlight", G_OBJECT (backlight));
}

/**
 * dkp_backlight_finalize:
 **/
static void
dkp_backlight_finalize (GObject *object)
{
	DkpBacklight *backlight;

	g_return_if_fail (object != NULL);
	g_return_if_fail (DKP_IS_BACKLIGHT (object));

	backlight = DKP_BACKLIGHT (object);
	backlight->priv = DKP_BACKLIGHT_GET_PRIVATE (backlight);

	if (backlight->priv->device != NULL)
		g_object_unref (backlight->priv->device);

	G_OBJECT_CLASS (dkp_backlight_parent_class)->finalize (object);
}

/**
 * dkp_backlight_new:
 **/
DkpBacklight *
dkp_backlight_new (void)
{
	DkpBacklight *backlight;
	backlight = g_object_new (DKP_TYPE_BACKLIGHT, NULL);
	return DKP_BACKLIGHT (backlight);
}

