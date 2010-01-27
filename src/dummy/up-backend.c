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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <glib/gi18n.h>
#include <gio/gio.h>

#include "egg-debug.h"

#include "up-backend.h"
#include "up-daemon.h"
#include "up-marshal.h"
#include "up-device.h"

static void	up_backend_class_init	(UpBackendClass	*klass);
static void	up_backend_init	(UpBackend		*backend);
static void	up_backend_finalize	(GObject		*object);

#define UP_BACKEND_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), UP_TYPE_BACKEND, UpBackendPrivate))

struct UpBackendPrivate
{
	UpDaemon		*daemon;
	UpDevice		*device;
	UpDeviceList		*device_list; /* unused */
	GObject			*native;
};

enum {
	SIGNAL_DEVICE_ADDED,
	SIGNAL_DEVICE_REMOVED,
	SIGNAL_LAST
};

static guint signals [SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE (UpBackend, up_backend, G_TYPE_OBJECT)

/**
 * up_backend_changed_time_cb:
 **/
static gboolean
up_backend_changed_time_cb (UpBackend *backend)
{
	UpDevice *device;
	GTimeVal timeval;

	//FIXME!
	device = NULL;

	/* reset time */
	g_get_current_time (&timeval);
	g_object_set (device, "update-time", (guint64) timeval.tv_sec, NULL);
	return TRUE;
}

/**
 * up_backend_add_cb:
 **/
static gboolean
up_backend_add_cb (UpBackend *backend)
{
	gboolean ret;

	/* coldplug */
	ret = up_device_coldplug (backend->priv->device, backend->priv->daemon, backend->priv->native);
	if (!ret) {
		egg_warning ("failed to coldplug");
		goto out;
	}

	/* emit */
	g_signal_emit (backend, signals[SIGNAL_DEVICE_ADDED], 0, backend->priv->native, backend->priv->device);

	/* setup poll */
	g_timeout_add_seconds (2, (GSourceFunc) up_backend_changed_time_cb, backend);
out:
	return FALSE;
}

/**
 * up_backend_coldplug:
 * @backend: The %UpBackend class instance
 * @daemon: The %UpDaemon controlling instance
 *
 * Finds all the devices already plugged in, and emits device-add signals for
 * each of them.
 *
 * Return value: %TRUE for success
 **/
gboolean
up_backend_coldplug (UpBackend *backend, UpDaemon *daemon)
{
	backend->priv->daemon = g_object_ref (daemon);
	backend->priv->device_list = up_daemon_get_device_list (daemon);

	/* small delay until first device is added */
	g_timeout_add_seconds (1, (GSourceFunc) up_backend_add_cb, backend);

	return TRUE;
}

/**
 * up_backend_class_init:
 * @klass: The UpBackendClass
 **/
static void
up_backend_class_init (UpBackendClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = up_backend_finalize;

	signals [SIGNAL_DEVICE_ADDED] =
		g_signal_new ("device-added",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (UpBackendClass, device_added),
			      NULL, NULL, up_marshal_VOID__POINTER_POINTER,
			      G_TYPE_NONE, 2, G_TYPE_POINTER, G_TYPE_POINTER);
	signals [SIGNAL_DEVICE_REMOVED] =
		g_signal_new ("device-removed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (UpBackendClass, device_removed),
			      NULL, NULL, up_marshal_VOID__POINTER_POINTER,
			      G_TYPE_NONE, 2, G_TYPE_POINTER, G_TYPE_POINTER);

	g_type_class_add_private (klass, sizeof (UpBackendPrivate));
}

/**
 * up_backend_init:
 **/
static void
up_backend_init (UpBackend *backend)
{
	backend->priv = UP_BACKEND_GET_PRIVATE (backend);
	backend->priv->daemon = NULL;
	backend->priv->device_list = NULL;
	backend->priv->native = g_object_new (UP_TYPE_DEVICE, NULL);
	backend->priv->device = up_device_new ();

	/* setup dummy */
	g_object_set (backend->priv->device,
		      "native-path", "/hal/blows/goats",
		      "vendor", "hughsie",
		      "model", "BAT1",
		      "serial", "0001",
		      "type", UP_DEVICE_KIND_BATTERY,
		      "online", FALSE,
		      "power-supply", TRUE,
		      "is-present", TRUE,
		      "is-rechargeable", TRUE,
		      "has-history", FALSE,
		      "has-statistics", FALSE,
		      "state", UP_DEVICE_STATE_DISCHARGING,
		      "energy", 0.0f,
		      "energy-empty", 0.0f,
		      "energy-full", 10.0f,
		      "energy-full-design", 10.0f,
		      "energy-rate", 5.0f,
		      "percentage", 50.0f,
		      NULL);
}

/**
 * up_backend_finalize:
 **/
static void
up_backend_finalize (GObject *object)
{
	UpBackend *backend;

	g_return_if_fail (UP_IS_BACKEND (object));

	backend = UP_BACKEND (object);

	if (backend->priv->daemon != NULL)
		g_object_unref (backend->priv->daemon);
	if (backend->priv->device_list != NULL)
		g_object_unref (backend->priv->device_list);

	g_object_unref (backend->priv->native);
	g_object_unref (backend->priv->device);

	G_OBJECT_CLASS (up_backend_parent_class)->finalize (object);
}

/**
 * up_backend_new:
 *
 * Return value: a new %UpBackend object.
 **/
UpBackend *
up_backend_new (void)
{
	UpBackend *backend;
	backend = g_object_new (UP_TYPE_BACKEND, NULL);
	return UP_BACKEND (backend);
}

/***************************************************************************
 ***                          MAKE CHECK TESTS                           ***
 ***************************************************************************/
#ifdef EGG_TEST
#include "egg-test.h"

void
up_backend_test (gpointer user_data)
{
	EggTest *test = (EggTest *) user_data;
	UpBackend *backend;

	if (!egg_test_start (test, "UpBackend"))
		return;

	/************************************************************/
	egg_test_title (test, "get instance");
	backend = up_backend_new ();
	egg_test_assert (test, backend != NULL);

	/* unref */
	g_object_unref (backend);

	egg_test_end (test);
}
#endif

