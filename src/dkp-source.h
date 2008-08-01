/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
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

#ifndef __DKP_SOURCE_H__
#define __DKP_SOURCE_H__

#include <glib-object.h>
#include <polkit-dbus/polkit-dbus.h>
#include <devkit-gobject.h>

#include "dkp-daemon.h"
#include "dkp-device.h"

G_BEGIN_DECLS

#define DKP_SOURCE_TYPE_SOURCE  (dkp_source_get_type ())
#define DKP_SOURCE(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), DKP_SOURCE_TYPE_SOURCE, DkpSource))
#define DKP_SOURCE_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), DKP_SOURCE_TYPE_SOURCE, DkpSourceClass))
#define DKP_IS_SOURCE(o)	(G_TYPE_CHECK_INSTANCE_TYPE ((o), DKP_SOURCE_TYPE_SOURCE))
#define DKP_IS_SOURCE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), DKP_SOURCE_TYPE_SOURCE))
#define DKP_SOURCE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), DKP_SOURCE_TYPE_SOURCE, DkpSourceClass))

typedef struct DkpSourcePrivate DkpSourcePrivate;

typedef struct
{
	DkpDevice		 parent;
	DkpSourcePrivate	*priv;
} DkpSource;

typedef struct
{
	DkpDeviceClass		 parent_class;
} DkpSourceClass;

GType		 dkp_source_get_type		(void);
DkpSource	*dkp_source_new			(DkpDaemon		*daemon,
						 DevkitDevice		*d);

/* exported methods */
gboolean	 dkp_source_refresh		(DkpSource		*power_source,
						 DBusGMethodInvocation	*context);
gchar		*dkp_source_get_id		(DkpSource		*power_source);

G_END_DECLS

#endif /* __DKP_SOURCE_H__ */
