/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2025 Kate Hsuan <p.hsuan@gmail.com>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>

#include "up-kbd-backlight.h"
#include "up-daemon.h"
#include "up-native.h"
#include "up-types.h"

static void     up_kbd_backlight_finalize   (GObject	*object);

struct UpKbdBacklightPrivate
{
	gint			 max_brightness;
	guint			 brightness;
};

G_DEFINE_TYPE_WITH_PRIVATE (UpKbdBacklight, up_kbd_backlight, UP_TYPE_DEVICE_KBD_BACKLIGHT)

/**
 * up_kbd_backlight_emit_change:
 **/
static void
up_kbd_backlight_emit_change(UpKbdBacklight *kbd_backlight, int value, const char *source)
{
	up_exported_kbd_backlight_emit_brightness_changed (UP_EXPORTED_KBD_BACKLIGHT (kbd_backlight), value);
	up_exported_kbd_backlight_emit_brightness_changed_with_source (UP_EXPORTED_KBD_BACKLIGHT (kbd_backlight), value, source);
}


/**
 * up_kbd_backlight_brightness_write:
 **/
static gboolean
up_kbd_backlight_brightness_write (UpDeviceKbdBacklight *kbd_backlight, const gchar *native_path, gint value)
{
	UpKbdBacklight *kbd;
	UpKbdBacklightPrivate *priv;
	g_autoptr (GString) value_str = g_string_new (NULL);

	g_return_val_if_fail (UP_IS_DEVICE_KBD_BACKLIGHT (kbd_backlight), FALSE);

	kbd = UP_KBD_BACKLIGHT (kbd_backlight);
	priv = up_kbd_backlight_get_instance_private (kbd);

	g_string_printf (value_str, "%d", CLAMP (value, 0, priv->max_brightness));
	if (!g_file_set_contents_full (native_path, value_str->str, value_str->len,
				       G_FILE_SET_CONTENTS_ONLY_EXISTING, 0644, NULL))
		return FALSE;

	return TRUE;
}

/**
 * up_kbd_backlight_brightness_read:
 **/
static gint
up_kbd_backlight_brightness_read (UpDeviceKbdBacklight *kbd_backlight, const gchar *native_path)
{
	GObject *native = NULL;
	g_autofree gchar *buf = NULL;
	gint64 brightness = -1;

	g_return_val_if_fail (UP_IS_DEVICE_KBD_BACKLIGHT (kbd_backlight), brightness);

	if (!g_file_get_contents (native_path, &buf, NULL, NULL))
		return -1;

	brightness = g_ascii_strtoll (buf, NULL, 10);
	if (brightness < 0) {
		g_warning ("failed to convert brightness: %s", buf);
		return -1;
	}

	return brightness;
}


/**
 * up_kbd_backlight_set_brightness:
 **/
static gboolean
up_kbd_backlight_set_brightness (UpDeviceKbdBacklight *kbd_backlight, gint value)
{
	GObject *native;
	g_autofree gchar *filename = NULL;
	const gchar *native_path;
	gboolean ret = FALSE;

	native = up_device_kbd_backlight_get_native (UP_DEVICE_KBD_BACKLIGHT (kbd_backlight));
	g_return_val_if_fail (native != NULL, FALSE);

	native_path = up_native_get_native_path (native);
	g_return_val_if_fail (native_path != NULL, FALSE);

	filename = g_build_filename (native_path, "brightness", NULL);

	g_debug ("setting brightness to %i", value);
	ret = up_kbd_backlight_brightness_write (kbd_backlight, filename, value);

	return ret;
}


/**
 * up_kbd_backlight_get_brightness:
 *
 * Gets the current brightness
 **/
static gint
up_kbd_backlight_get_brightness (UpDeviceKbdBacklight *kbd_backlight)
{
	GObject *native;
	const gchar *native_path;
	g_autofree gchar *filename = NULL;
	gint brightness = -1;

	native = up_device_kbd_backlight_get_native (UP_DEVICE_KBD_BACKLIGHT (kbd_backlight));
	g_return_val_if_fail (native != NULL, brightness);

	native_path = up_native_get_native_path (native);
	g_return_val_if_fail (native_path != NULL, brightness);

	filename = g_build_filename (native_path, "brightness", NULL);
	brightness = up_kbd_backlight_brightness_read (kbd_backlight, filename);

	return brightness;
}

/**
 * up_kbd_backlight_get_max_brightness:
 *
 * Gets the max brightness
 **/
static gint
up_kbd_backlight_get_max_brightness (UpDeviceKbdBacklight *kbd_backlight)
{
	UpKbdBacklight *kbd = UP_KBD_BACKLIGHT (kbd_backlight);
	UpKbdBacklightPrivate *priv = up_kbd_backlight_get_instance_private (kbd);
	return priv->max_brightness;
}

/**
 * up_kbd_backlight_coldplug:
 *
 * Initial max brightness
 **/
static gboolean
up_kbd_backlight_coldplug (UpDeviceKbdBacklight *kbd_backlight)
{
	UpKbdBacklight *kbd;
	UpKbdBacklightPrivate *priv;
	GObject *native;
	g_autofree gchar *filename = NULL;
	const gchar *native_path = NULL;

	g_return_val_if_fail (UP_IS_DEVICE_KBD_BACKLIGHT (kbd_backlight), FALSE);

	kbd = UP_KBD_BACKLIGHT (kbd_backlight);
	priv = up_kbd_backlight_get_instance_private (kbd);

	native = up_device_kbd_backlight_get_native (kbd_backlight);
	if (native == NULL) {
		priv->max_brightness = 0;
		return FALSE;
	}

	g_debug ("coldplug kbd backlight");

	native_path = up_native_get_native_path (native);
	filename = g_build_filename (native_path, "max_brightness", NULL);
	g_debug ("LED Max brightness path %s", filename);

	priv->max_brightness = up_kbd_backlight_brightness_read (kbd_backlight, filename);

	return TRUE;
}


/**
 * up_kbd_backlight_finalize:
 **/
static void
up_kbd_backlight_finalize (GObject *object)
{
	UpKbdBacklight *kbd_backlight;

#if 0
	g_return_if_fail (object != NULL);
	g_return_if_fail (UP_IS_KBD_BACKLIGHT (object));

	kbd_backlight = UP_KBD_BACKLIGHT (object);
	kbd_backlight->priv = up_kbd_backlight_get_instance_private (kbd_backlight);

	if (kbd_backlight->priv->channel_hw_changed) {
		g_io_channel_shutdown (kbd_backlight->priv->channel_hw_changed, FALSE, NULL);
		g_io_channel_unref (kbd_backlight->priv->channel_hw_changed);
	}

	if (kbd_backlight->priv->fd_hw_changed >= 0)
		close (kbd_backlight->priv->fd_hw_changed);

	/* close file */
	if (kbd_backlight->priv->fd >= 0)
		close (kbd_backlight->priv->fd);

	G_OBJECT_CLASS (up_kbd_backlight_parent_class)->finalize (object);
#endif
}

/**
 * up_kbd_backlight_class_init:
 **/
static void
up_kbd_backlight_class_init (UpKbdBacklightClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	UpDeviceKbdBacklightClass *dev_kbd_klass = UP_DEVICE_KBD_BACKLIGHT_CLASS (klass);

	object_class->finalize = up_kbd_backlight_finalize;

	dev_kbd_klass->coldplug = up_kbd_backlight_coldplug;
	dev_kbd_klass->get_max_brightness = up_kbd_backlight_get_max_brightness;
	dev_kbd_klass->get_brightness = up_kbd_backlight_get_brightness;
	dev_kbd_klass->set_brightness = up_kbd_backlight_set_brightness;
}


/**
 * up_kbd_backlight_init:
 **/
static void
up_kbd_backlight_init (UpKbdBacklight *kbd_backlight)
{
	UpKbdBacklightPrivate *priv = up_kbd_backlight_get_instance_private (kbd_backlight);

	kbd_backlight->priv = up_kbd_backlight_get_instance_private (kbd_backlight);
}

/**
 * up_kbd_backlight_new:
 **/
UpKbdBacklight *
up_kbd_backlight_new (void)
{
	return g_object_new (UP_TYPE_KBD_BACKLIGHT, NULL);
}
