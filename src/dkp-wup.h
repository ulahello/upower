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

#ifndef __DKP_WUP_H__
#define __DKP_WUP_H__

#include <glib-object.h>
#include "dkp-device.h"

G_BEGIN_DECLS

#define DKP_TYPE_WUP  		(dkp_wup_get_type ())
#define DKP_WUP(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), DKP_TYPE_WUP, DkpWup))
#define DKP_WUP_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), DKP_TYPE_WUP, DkpWupClass))
#define DKP_IS_WUP(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), DKP_TYPE_WUP))
#define DKP_IS_WUP_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), DKP_TYPE_WUP))
#define DKP_WUP_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), DKP_TYPE_WUP, DkpWupClass))

typedef struct DkpWupPrivate DkpWupPrivate;

typedef struct
{
	DkpDevice		 parent;
	DkpWupPrivate		*priv;
} DkpWup;

typedef struct
{
	DkpDeviceClass		 parent_class;
} DkpWupClass;

GType		 dkp_wup_get_type		(void);
DkpWup		*dkp_wup_new			(void);

G_END_DECLS

#endif /* __DKP_WUP_H__ */

