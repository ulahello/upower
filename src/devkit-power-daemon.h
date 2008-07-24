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

#ifndef __DEVKIT_POWER_DAEMON_H__
#define __DEVKIT_POWER_DAEMON_H__

#include <glib-object.h>
#include <polkit-dbus/polkit-dbus.h>
#include <dbus/dbus-glib.h>

G_BEGIN_DECLS

#define DEVKIT_TYPE_POWER_DAEMON         (devkit_power_daemon_get_type ())
#define DEVKIT_POWER_DAEMON(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), DEVKIT_TYPE_POWER_DAEMON, DevkitPowerDaemon))
#define DEVKIT_POWER_DAEMON_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), DEVKIT_TYPE_POWER_DAEMON, DevkitPowerDaemonClass))
#define DEVKIT_IS_POWER_DAEMON(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), DEVKIT_TYPE_POWER_DAEMON))
#define DEVKIT_IS_POWER_DAEMON_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), DEVKIT_TYPE_POWER_DAEMON))
#define DEVKIT_POWER_DAEMON_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), DEVKIT_TYPE_POWER_DAEMON, DevkitPowerDaemonClass))

typedef struct DevkitPowerDaemonPrivate DevkitPowerDaemonPrivate;

typedef struct
{
        GObject        parent;
        DevkitPowerDaemonPrivate *priv;
} DevkitPowerDaemon;

typedef struct
{
        GObjectClass   parent_class;
} DevkitPowerDaemonClass;

typedef enum
{
        DEVKIT_POWER_DAEMON_ERROR_GENERAL,
        DEVKIT_POWER_DAEMON_ERROR_NOT_SUPPORTED,
        DEVKIT_POWER_DAEMON_ERROR_NO_SUCH_DEVICE,
        DEVKIT_POWER_DAEMON_NUM_ERRORS
} DevkitPowerDaemonError;

#define DEVKIT_POWER_DAEMON_ERROR devkit_power_daemon_error_quark ()

GType devkit_power_daemon_error_get_type (void);
#define DEVKIT_POWER_DAEMON_TYPE_ERROR (devkit_power_daemon_error_get_type ())

GQuark             devkit_power_daemon_error_quark         (void);
GType              devkit_power_daemon_get_type            (void);
DevkitPowerDaemon *devkit_power_daemon_new                 (void);

/* local methods */

PolKitCaller *devkit_power_damon_local_get_caller_for_context (DevkitPowerDaemon     *daemon,
                                                               DBusGMethodInvocation *context);

gboolean      devkit_power_damon_local_check_auth (DevkitPowerDaemon     *daemon,
                                                   PolKitCaller          *pk_caller,
                                                   const char            *action_id,
                                                   DBusGMethodInvocation *context);

/* exported methods */

gboolean devkit_power_daemon_enumerate_devices (DevkitPowerDaemon     *daemon,
                                                DBusGMethodInvocation *context);
gboolean devkit_power_daemon_suspend (DevkitPowerDaemon     *daemon,
                                      DBusGMethodInvocation *context);
gboolean devkit_power_daemon_hibernate (DevkitPowerDaemon     *daemon,
                                        DBusGMethodInvocation *context);

G_END_DECLS

#endif /* __DEVKIT_POWER_DAEMON_H__ */
