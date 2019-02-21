/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2017 Bastien Nocera <hadess@hadess.net>
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

#include <gio/gio.h>

#include "up-types.h"
#include "up-device-bluez.h"

G_DEFINE_TYPE (UpDeviceBluez, up_device_bluez, UP_TYPE_DEVICE)

static UpDeviceKind
appearance_to_kind (guint16 appearance)
{
        switch ((appearance & 0xffc0) >> 6) {
        case 0x01:
                return UP_DEVICE_KIND_PHONE;
        case 0x02:
                return UP_DEVICE_KIND_COMPUTER;
        case 0x05:
                return UP_DEVICE_KIND_MONITOR;
        case 0x0a:
                return UP_DEVICE_KIND_MEDIA_PLAYER;
        case 0x0f: /* HID Generic */
                switch (appearance & 0x3f) {
                case 0x01:
                        return UP_DEVICE_KIND_KEYBOARD;
                case 0x02:
                        return UP_DEVICE_KIND_MOUSE;
                case 0x03:
                case 0x04:
                        return UP_DEVICE_KIND_GAMING_INPUT;
                case 0x05:
                        return UP_DEVICE_KIND_TABLET;
                }
                break;
        }

	return UP_DEVICE_KIND_UNKNOWN;
}

/**
 * up_device_bluez_coldplug:
 *
 * Return %TRUE on success, %FALSE if we failed to get data and should be removed
 **/
static gboolean
up_device_bluez_coldplug (UpDevice *device)
{
	GDBusObjectProxy *object_proxy;
	GDBusProxy *proxy;
	GError *error = NULL;
	UpDeviceKind kind;
	const char *uuid;
	const char *model;
	guint16 appearance;
	guchar percentage;

	/* Static device properties */
	object_proxy = G_DBUS_OBJECT_PROXY (up_device_get_native (device));
	proxy = g_dbus_proxy_new_sync (g_dbus_object_proxy_get_connection (object_proxy),
				       G_DBUS_PROXY_FLAGS_NONE,
				       NULL,
				       "org.bluez",
				       g_dbus_object_get_object_path (G_DBUS_OBJECT (object_proxy)),
				       "org.bluez.Device1",
				       NULL,
				       &error);

	if (!proxy) {
		g_warning ("Failed to get proxy for %s (iface org.bluez.Device1)",
			   g_dbus_object_get_object_path (G_DBUS_OBJECT (object_proxy)));
		return FALSE;
	}

	appearance = g_variant_get_uint16 (g_dbus_proxy_get_cached_property (proxy, "Appearance"));
	kind = appearance_to_kind (appearance);
	uuid = g_variant_get_string (g_dbus_proxy_get_cached_property (proxy, "Address"), NULL);
	model = g_variant_get_string (g_dbus_proxy_get_cached_property (proxy, "Alias"), NULL);

	/* hardcode some values */
	g_object_set (device,
		      "type", kind,
		      "serial", uuid,
		      "model", model,
		      "power-supply", FALSE,
		      "has-history", TRUE,
		      NULL);

	g_object_unref (proxy);

	/* Initial battery values */
	proxy = g_dbus_proxy_new_sync (g_dbus_object_proxy_get_connection (object_proxy),
				       G_DBUS_PROXY_FLAGS_NONE,
				       NULL,
				       "org.bluez",
				       g_dbus_object_get_object_path (G_DBUS_OBJECT (object_proxy)),
				       "org.bluez.Battery1",
				       NULL,
				       &error);

	if (!proxy) {
		g_warning ("Failed to get proxy for %s",
			   g_dbus_object_get_object_path (G_DBUS_OBJECT (object_proxy)));
		return FALSE;
	}

	percentage = g_variant_get_byte (g_dbus_proxy_get_cached_property (proxy, "Percentage"));

	g_object_set (device,
		      "is-present", TRUE,
		      "percentage", (gdouble) percentage,
		      NULL);

	g_object_unref (proxy);

	return TRUE;
}

static void
up_device_bluez_init (UpDeviceBluez *bluez)
{
}

void
up_device_bluez_update (UpDeviceBluez *bluez,
			GVariant      *properties)
{
	UpDevice *device = UP_DEVICE (bluez);
	GVariantIter iter;
	const gchar *key;
	GVariant *value;

	g_variant_iter_init (&iter, properties);
	while (g_variant_iter_next (&iter, "{&sv}", &key, &value)) {
		if (g_str_equal (key, "Percentage")) {
			g_object_set (device, "percentage", (gdouble) g_variant_get_byte (value), NULL);
		} else {
			char *str = g_variant_print (value, TRUE);

			g_warning ("Unhandled key: %s value: %s", key, str);
			g_free (str);
		}
		g_variant_unref (value);
	}
}

static void
up_device_bluez_class_init (UpDeviceBluezClass *klass)
{
	UpDeviceClass *device_class = UP_DEVICE_CLASS (klass);

	device_class->coldplug = up_device_bluez_coldplug;
}

UpDeviceBluez *
up_device_bluez_new (void)
{
	return g_object_new (UP_TYPE_DEVICE_BLUEZ, NULL);
}

