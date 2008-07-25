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

#ifndef __DEVKIT_POWER_SOURCE_H__
#define __DEVKIT_POWER_SOURCE_H__

#include <glib-object.h>
#include <polkit-dbus/polkit-dbus.h>
#include <devkit-gobject.h>

#include "devkit-power-daemon.h"
#include "devkit-power-device.h"

G_BEGIN_DECLS

#define DEVKIT_TYPE_POWER_SOURCE         (devkit_power_source_get_type ())
#define DEVKIT_POWER_SOURCE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), DEVKIT_TYPE_POWER_SOURCE, DevkitPowerSource))
#define DEVKIT_POWER_SOURCE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), DEVKIT_TYPE_POWER_SOURCE, DevkitPowerSourceClass))
#define DEVKIT_IS_POWER_SOURCE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), DEVKIT_TYPE_POWER_SOURCE))
#define DEVKIT_IS_POWER_SOURCE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), DEVKIT_TYPE_POWER_SOURCE))
#define DEVKIT_POWER_SOURCE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), DEVKIT_TYPE_POWER_SOURCE, DevkitPowerSourceClass))

typedef struct DevkitPowerSourcePrivate DevkitPowerSourcePrivate;

typedef struct
{
        DevkitPowerDevice         parent;
        DevkitPowerSourcePrivate *priv;
} DevkitPowerSource;

typedef struct
{
        DevkitPowerDeviceClass    parent_class;
} DevkitPowerSourceClass;

GType              devkit_power_source_get_type   (void);
DevkitPowerSource *devkit_power_source_new        (DevkitPowerDaemon *daemon,
                                                   DevkitDevice      *d);

/* exported methods */

gboolean           devkit_power_source_refresh    (DevkitPowerSource     *power_source,
                                                   DBusGMethodInvocation *context);
gchar             *devkit_power_source_get_id     (DevkitPowerSource     *power_source);

G_END_DECLS

#endif /* __DEVKIT_POWER_SOURCE_H__ */
