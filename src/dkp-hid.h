/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 Richard Hughes <richard@hughsie.com>
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

#ifndef __DKP_HID_H__
#define __DKP_HID_H__

#include <glib-object.h>
#include "dkp-device.h"

G_BEGIN_DECLS

#define DKP_TYPE_HID  		(dkp_hid_get_type ())
#define DKP_HID(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), DKP_TYPE_HID, DkpHid))
#define DKP_HID_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), DKP_TYPE_HID, DkpHidClass))
#define DKP_IS_HID(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), DKP_TYPE_HID))
#define DKP_IS_HID_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), DKP_TYPE_HID))
#define DKP_HID_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), DKP_TYPE_HID, DkpHidClass))

typedef struct DkpHidPrivate DkpHidPrivate;

typedef struct
{
	DkpDevice		 parent;
	DkpHidPrivate		*priv;
} DkpHid;

typedef struct
{
	DkpDeviceClass		 parent_class;
} DkpHidClass;

GType		 dkp_hid_get_type		(void);
DkpHid		*dkp_hid_new			(void);

G_END_DECLS

#endif /* __DKP_HID_H__ */

