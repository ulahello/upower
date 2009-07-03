/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008-2009 Richard Hughes <richard@hughsie.com>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __DKP_BACKLIGHT_H
#define __DKP_BACKLIGHT_H

#include <glib-object.h>
#include <devkit-gobject/devkit-gobject.h>

G_BEGIN_DECLS

#define DKP_TYPE_BACKLIGHT		(dkp_backlight_get_type ())
#define DKP_BACKLIGHT(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), DKP_TYPE_BACKLIGHT, DkpBacklight))
#define DKP_BACKLIGHT_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), DKP_TYPE_BACKLIGHT, DkpBacklightClass))
#define DKP_IS_BACKLIGHT(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), DKP_TYPE_BACKLIGHT))
#define DKP_IS_BACKLIGHT_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), DKP_TYPE_BACKLIGHT))
#define DKP_BACKLIGHT_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), DKP_TYPE_BACKLIGHT, DkpBacklightClass))

typedef struct DkpBacklightPrivate DkpBacklightPrivate;

typedef struct
{
	GObject			  parent;
	DkpBacklightPrivate	 *priv;
} DkpBacklight;

typedef struct
{
	GObjectClass	parent_class;
	void		(* brightness_changed)		(DkpBacklight	*backlight,
							 guint		 value);
} DkpBacklightClass;

DkpBacklight	*dkp_backlight_new			(void);
GType		 dkp_backlight_get_type			(void);

gboolean	 dkp_backlight_set_device		(DkpBacklight	*backlight,
							 DevkitDevice	*d);
gboolean	 dkp_backlight_changed			(DkpBacklight	*backlight);

/* exported methods */
void		 dkp_backlight_set_brightness		(DkpBacklight	*backlight,
							 guint		 value,
							 DBusGMethodInvocation *context);

G_END_DECLS

#endif	/* __DKP_BACKLIGHT_H */
