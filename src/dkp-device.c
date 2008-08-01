/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 David Zeuthen <david@fubar.dk>
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

#include <string.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n-lib.h>
#include <glib-object.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <devkit-gobject.h>
#include <polkit-dbus/polkit-dbus.h>

#include "sysfs-utils.h"
#include "dkp-device.h"
#include "dkp-source.h"

static void     dkp_device_class_init  (DkpDeviceClass *klass);
static void     dkp_device_init        (DkpDevice      *seat);

G_DEFINE_TYPE (DkpDevice, dkp_device, G_TYPE_OBJECT)

#define DKP_DEVICE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), DKP_SOURCE_TYPE_DEVICE, DkpDevicePrivate))

static void
dkp_device_class_init (DkpDeviceClass *klass)
{
}

static void
dkp_device_init (DkpDevice *device)
{
}

void
dkp_device_removed (DkpDevice *device)
{
        DkpDeviceClass *klass = DKP_DEVICE_GET_CLASS (device);
        klass->removed (device);
}

DkpDevice *
dkp_device_new (DkpDaemon *daemon, DevkitDevice *d)
{
        const char *subsys;
        DkpDevice *device;

        device = NULL;

        subsys = devkit_device_get_subsystem (d);
        if (strcmp (subsys, "power_supply") == 0) {
                device = DKP_DEVICE (dkp_source_new (daemon, d));
        }

        return device;
}

gboolean
dkp_device_changed (DkpDevice *device, DevkitDevice *d, gboolean synthesized)
{
        DkpDeviceClass *klass = DKP_DEVICE_GET_CLASS (device);
        return (klass->changed (device, d, synthesized));
}

const char *
dkp_device_get_object_path (DkpDevice *device)
{
        DkpDeviceClass *klass = DKP_DEVICE_GET_CLASS (device);
        return (klass->get_object_path (device));
}
