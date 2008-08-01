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

static void     devkit_power_device_class_init  (DevkitPowerDeviceClass *klass);
static void     devkit_power_device_init        (DevkitPowerDevice      *seat);

G_DEFINE_TYPE (DevkitPowerDevice, devkit_power_device, G_TYPE_OBJECT)

#define DEVKIT_POWER_DEVICE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), DEVKIT_TYPE_POWER_DEVICE, DevkitPowerDevicePrivate))

static void
devkit_power_device_class_init (DevkitPowerDeviceClass *klass)
{
}

static void
devkit_power_device_init (DevkitPowerDevice *device)
{
}

void
devkit_power_device_removed (DevkitPowerDevice *device)
{
        DevkitPowerDeviceClass *klass = DEVKIT_POWER_DEVICE_GET_CLASS (device);
        klass->removed (device);
}

DevkitPowerDevice *
devkit_power_device_new (DevkitPowerDaemon *daemon, DevkitDevice *d)
{
        const char *subsys;
        DevkitPowerDevice *device;

        device = NULL;

        subsys = devkit_device_get_subsystem (d);
        if (strcmp (subsys, "power_supply") == 0) {
                device = DEVKIT_POWER_DEVICE (devkit_power_source_new (daemon, d));
        }

        return device;
}

gboolean
devkit_power_device_changed (DevkitPowerDevice *device, DevkitDevice *d, gboolean synthesized)
{
        DevkitPowerDeviceClass *klass = DEVKIT_POWER_DEVICE_GET_CLASS (device);
        return (klass->changed (device, d, synthesized));
}

const char *
devkit_power_device_get_object_path (DevkitPowerDevice *device)
{
        DevkitPowerDeviceClass *klass = DEVKIT_POWER_DEVICE_GET_CLASS (device);
        return (klass->get_object_path (device));
}
