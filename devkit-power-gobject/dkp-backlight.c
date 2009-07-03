/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009 Richard Hughes <richard@hughsie.com>
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
#include <dbus/dbus-glib.h>

#include "dkp-backlight.h"

static void	dkp_backlight_class_init	(DkpBacklightClass	*klass);
static void	dkp_backlight_init	(DkpBacklight		*backlight);
static void	dkp_backlight_finalize	(GObject		*object);

#define DKP_BACKLIGHT_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), DKP_TYPE_BACKLIGHT, DkpBacklightPrivate))

struct DkpBacklightPrivate
{
	DBusGConnection		*bus;
	DBusGProxy		*proxy;
	DBusGProxy		*prop_proxy;
	gboolean		 have_interface;
	gboolean		 have_properties;
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
 * dkp_backlight_set_brightness:
 *
 * Sets the backlight brightness
 **/
gboolean
dkp_backlight_set_brightness (DkpBacklight *backlight, guint value, GError **error)
{
	GError *error_local = NULL;
	gboolean ret = FALSE;

	g_return_val_if_fail (DKP_IS_BACKLIGHT (backlight), FALSE);
	g_return_val_if_fail (backlight->priv->proxy != NULL, FALSE);

	/* no device */
	if (!backlight->priv->have_interface) {
		if (error != NULL)
			*error = g_error_new (1, 0, "no device to control");
		goto out;
	}

	/* set brightness */
	ret = dbus_g_proxy_call (backlight->priv->proxy, "SetBrightness", &error_local,
				 G_TYPE_UINT, value,
				 G_TYPE_INVALID,
				 G_TYPE_INVALID);
	if (!ret) {
		g_warning ("SetBrightness failed: %s", error_local->message);
		if (error != NULL)
			*error = g_error_new (1, 0, "%s", error_local->message);
		g_error_free (error_local);
		goto out;
	}
out:
	return ret;
}

/**
 * dkp_backlight_ensure_properties:
 **/
static void
dkp_backlight_ensure_properties (DkpBacklight *backlight)
{
	gboolean ret;
	GError *error;
	GHashTable *props;
	GValue *value;

	props = NULL;

	if (backlight->priv->have_properties)
		goto out;

	/* no device */
	if (!backlight->priv->have_interface)
		goto out;

	error = NULL;
	ret = dbus_g_proxy_call (backlight->priv->prop_proxy, "GetAll", &error,
				 G_TYPE_STRING, "org.freedesktop.DeviceKit.Power.Backlight.Temporary.Interface",
				 G_TYPE_INVALID,
				 dbus_g_type_get_map ("GHashTable", G_TYPE_STRING, G_TYPE_VALUE), &props,
				 G_TYPE_INVALID);
	if (!ret) {
		g_debug ("Error invoking GetAll() to get properties: %s", error->message);
		backlight->priv->have_interface = FALSE;
		g_error_free (error);
		goto out;
	}

	value = g_hash_table_lookup (props, "action-in-hardware");
	if (value == NULL) {
		g_warning ("No 'action-in-hardware' property");
		goto out;
	}
	backlight->priv->action_in_hardware = g_value_get_boolean (value);

	value = g_hash_table_lookup (props, "actual");
	if (value == NULL) {
		g_warning ("No 'actual' property");
		goto out;
	}
	backlight->priv->actual = g_value_get_uint (value);

	value = g_hash_table_lookup (props, "maximum");
	if (value == NULL) {
		g_warning ("No 'maximum' property");
		goto out;
	}
	backlight->priv->maximum = g_value_get_uint (value);

	/* cached */
	backlight->priv->have_properties = TRUE;

out:
	if (props != NULL)
		g_hash_table_unref (props);
}

/**
 * dkp_backlight_brightness_changed_cb:
 **/
static void
dkp_backlight_brightness_changed_cb (DBusGProxy *proxy, guint value, DkpBacklight *backlight)
{
	backlight->priv->have_properties = FALSE;
	g_signal_emit (backlight, signals [BRIGHTNESS_CHANGED], 0, value);
}

/**
 * dkp_backlight_get_property:
 **/
static void
dkp_backlight_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	DkpBacklight *backlight = DKP_BACKLIGHT (object);

	dkp_backlight_ensure_properties (backlight);

	switch (prop_id) {
	case PROP_ACTUAL:
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
 * @klass: The DkpBacklightClass
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

	g_type_class_add_private (klass, sizeof (DkpBacklightPrivate));
}

/**
 * dkp_backlight_init:
 * @backlight: This class instance
 **/
static void
dkp_backlight_init (DkpBacklight *backlight)
{
	GError *error = NULL;

	backlight->priv = DKP_BACKLIGHT_GET_PRIVATE (backlight);
	backlight->priv->have_properties = FALSE;
	backlight->priv->actual = 0;
	backlight->priv->maximum = 0;
	backlight->priv->action_in_hardware = FALSE;
	backlight->priv->have_interface = TRUE;

	/* get on the bus */
	backlight->priv->bus = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
	if (backlight->priv->bus == NULL) {
		g_warning ("Couldn't connect to system bus: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* connect to properties interface */
	backlight->priv->prop_proxy = dbus_g_proxy_new_for_name (backlight->priv->bus,
							      "org.freedesktop.DeviceKit.Power",
							      "/org/freedesktop/DeviceKit/Power/Backlight",
							      "org.freedesktop.DBus.Properties");
	if (backlight->priv->prop_proxy == NULL) {
		g_warning ("Couldn't connect to proxy");
		goto out;
	}

	/* connect to main interface */
	backlight->priv->proxy = dbus_g_proxy_new_for_name (backlight->priv->bus,
							 "org.freedesktop.DeviceKit.Power",
							 "/org/freedesktop/DeviceKit/Power/Backlight",
							 "org.freedesktop.DeviceKit.Power.Backlight.Temporary.Interface");
	if (backlight->priv->proxy == NULL) {
		g_warning ("Couldn't connect to proxy");
		goto out;
	}
	dbus_g_proxy_add_signal (backlight->priv->proxy, "BrightnessChanged", G_TYPE_UINT, G_TYPE_INVALID);

	/* all callbacks */
	dbus_g_proxy_connect_signal (backlight->priv->proxy, "BrightnessChanged",
				     G_CALLBACK (dkp_backlight_brightness_changed_cb), backlight, NULL);
out:
	return;
}

/**
 * dkp_backlight_finalize:
 * @object: The object to finalize
 **/
static void
dkp_backlight_finalize (GObject *object)
{
	DkpBacklight *backlight;

	g_return_if_fail (DKP_IS_BACKLIGHT (object));

	backlight = DKP_BACKLIGHT (object);
	if (backlight->priv->proxy != NULL)
		g_object_unref (backlight->priv->proxy);
	if (backlight->priv->prop_proxy != NULL)
		g_object_unref (backlight->priv->prop_proxy);

	G_OBJECT_CLASS (dkp_backlight_parent_class)->finalize (object);
}

/**
 * dkp_backlight_new:
 *
 * Return value: a new DkpBacklight object.
 **/
DkpBacklight *
dkp_backlight_new (void)
{
	DkpBacklight *backlight;
	backlight = g_object_new (DKP_TYPE_BACKLIGHT, NULL);
	return DKP_BACKLIGHT (backlight);
}

