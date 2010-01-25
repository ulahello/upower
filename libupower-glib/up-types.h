/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 Richard Hughes <richard@hughsie.com>
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

#if !defined (__UPOWER_H_INSIDE__) && !defined (UP_COMPILATION)
#error "Only <upower.h> can be included directly."
#endif

#ifndef __UP_TYPES_H
#define __UP_TYPES_H

#include <glib-object.h>

G_BEGIN_DECLS

typedef enum {
	UP_DEVICE_TYPE_UNKNOWN,
	UP_DEVICE_TYPE_LINE_POWER,
	UP_DEVICE_TYPE_BATTERY,
	UP_DEVICE_TYPE_UPS,
	UP_DEVICE_TYPE_MONITOR,
	UP_DEVICE_TYPE_MOUSE,
	UP_DEVICE_TYPE_KEYBOARD,
	UP_DEVICE_TYPE_PDA,
	UP_DEVICE_TYPE_PHONE,
	UP_DEVICE_TYPE_LAST
} UpDeviceType;

typedef enum {
	UP_DEVICE_STATE_UNKNOWN,
	UP_DEVICE_STATE_CHARGING,
	UP_DEVICE_STATE_DISCHARGING,
	UP_DEVICE_STATE_EMPTY,
	UP_DEVICE_STATE_FULLY_CHARGED,
	UP_DEVICE_STATE_PENDING_CHARGE,
	UP_DEVICE_STATE_PENDING_DISCHARGE,
	UP_DEVICE_STATE_LAST
} UpDeviceState;

typedef enum {
	UP_DEVICE_TECHNOLOGY_UNKNOWN,
	UP_DEVICE_TECHNOLOGY_LITHIUM_ION,
	UP_DEVICE_TECHNOLOGY_LITHIUM_POLYMER,
	UP_DEVICE_TECHNOLOGY_LITHIUM_IRON_PHOSPHATE,
	UP_DEVICE_TECHNOLOGY_LEAD_ACID,
	UP_DEVICE_TECHNOLOGY_NICKEL_CADMIUM,
	UP_DEVICE_TECHNOLOGY_NICKEL_METAL_HYDRIDE,
	UP_DEVICE_TECHNOLOGY_LAST
} UpDeviceTechnology;

typedef enum {
	UP_QOS_TYPE_UNKNOWN,
	UP_QOS_TYPE_NETWORK,
	UP_QOS_TYPE_CPU_DMA,
	UP_QOS_TYPE_LAST
} UpQosType;

const gchar	*up_device_type_to_string		(UpDeviceType		 type_enum);
const gchar	*up_device_state_to_string		(UpDeviceState		 state_enum);
const gchar	*up_device_technology_to_string		(UpDeviceTechnology	 technology_enum);
UpDeviceType	 up_device_type_from_string		(const gchar		*type);
UpDeviceState	 up_device_state_from_string		(const gchar		*state);
UpDeviceTechnology up_device_technology_from_string	(const gchar		*technology);
const gchar	*up_qos_type_to_string			(UpQosType		 type);
UpQosType	 up_qos_type_from_string		(const gchar		*type);

G_END_DECLS

#endif /* __UP_TYPES_H */

