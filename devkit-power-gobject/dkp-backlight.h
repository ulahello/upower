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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#if !defined (__DEVICEKIT_POWER_H_INSIDE__) && !defined (DKP_COMPILATION)
#error "Only <devicekit-power.h> can be included directly."
#endif

#if !defined (I_KNOW_DKP_BACKLIGHT_IS_TEMPORARY) && !defined (DKP_COMPILATION)
#error You have to define I_KNOW_DKP_BACKLIGHT_IS_TEMPORARY to use this code
#endif

#ifndef __DKP_BACKLIGHT_H
#define __DKP_BACKLIGHT_H

#include <glib-object.h>

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
	 GObject		 parent;
	 DkpBacklightPrivate	*priv;
} DkpBacklight;

typedef struct
{
	GObjectClass		 parent_class;
	void			(*brightness_changed)	(DkpBacklight		*backlight,
							 guint			 value);
} DkpBacklightClass;

GType		 dkp_backlight_get_type			(void);
DkpBacklight	*dkp_backlight_new			(void);
gboolean	 dkp_backlight_set_brightness		(DkpBacklight		*backlight,
							 guint			 value,
							 GError			**error);

G_END_DECLS

#endif /* __DKP_BACKLIGHT_H */

