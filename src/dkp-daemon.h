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

#ifndef __DKP_DAEMON_H__
#define __DKP_DAEMON_H__

#include <glib-object.h>
#include <polkit-dbus/polkit-dbus.h>
#include <dbus/dbus-glib.h>

G_BEGIN_DECLS

#define DKP_SOURCE_TYPE_DAEMON         (dkp_daemon_get_type ())
#define DKP_DAEMON(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), DKP_SOURCE_TYPE_DAEMON, DkpDaemon))
#define DKP_DAEMON_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), DKP_SOURCE_TYPE_DAEMON, DkpDaemonClass))
#define DKP_IS_DAEMON(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), DKP_SOURCE_TYPE_DAEMON))
#define DKP_IS_DAEMON_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), DKP_SOURCE_TYPE_DAEMON))
#define DKP_DAEMON_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), DKP_SOURCE_TYPE_DAEMON, DkpDaemonClass))

typedef struct DkpDaemonPrivate DkpDaemonPrivate;

typedef struct
{
        GObject        parent;
        DkpDaemonPrivate *priv;
} DkpDaemon;

typedef struct
{
        GObjectClass   parent_class;
} DkpDaemonClass;

typedef enum
{
        DKP_DAEMON_ERROR_GENERAL,
        DKP_DAEMON_ERROR_NOT_SUPPORTED,
        DKP_DAEMON_ERROR_NO_SUCH_DEVICE,
        DKP_DAEMON_NUM_ERRORS
} DkpDaemonError;

#define DKP_DAEMON_ERROR dkp_daemon_error_quark ()

GType dkp_daemon_error_get_type (void);
#define DKP_DAEMON_TYPE_ERROR (dkp_daemon_error_get_type ())

GQuark             dkp_daemon_error_quark         (void);
GType              dkp_daemon_get_type            (void);
DkpDaemon *dkp_daemon_new                 (void);

/* local methods */

PolKitCaller *dkp_daemon_local_get_caller_for_context (DkpDaemon     *daemon,
                                                               DBusGMethodInvocation *context);

gboolean      dkp_daemon_local_check_auth (DkpDaemon     *daemon,
                                                   PolKitCaller          *pk_caller,
                                                   const char            *action_id,
                                                   DBusGMethodInvocation *context);

/* exported methods */

gboolean dkp_daemon_enumerate_devices (DkpDaemon     *daemon,
                                                DBusGMethodInvocation *context);
gboolean dkp_daemon_get_on_battery (DkpDaemon     *daemon,
                                             DBusGMethodInvocation *context);
gboolean dkp_daemon_get_low_battery (DkpDaemon     *daemon,
                                              DBusGMethodInvocation *context);
gboolean dkp_daemon_suspend (DkpDaemon     *daemon,
                                      DBusGMethodInvocation *context);
gboolean dkp_daemon_hibernate (DkpDaemon     *daemon,
                                        DBusGMethodInvocation *context);

G_END_DECLS

#endif /* __DKP_DAEMON_H__ */
