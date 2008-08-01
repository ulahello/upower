/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 David Zeuthen <david@fubar.dk>
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

#ifndef __DKP_ENUM_H__
#define __DKP_ENUM_H__

#include <glib.h>

G_BEGIN_DECLS

typedef enum {
        DKP_SOURCE_TYPE_LINE_POWER,
        DKP_SOURCE_TYPE_BATTERY,
        DKP_SOURCE_TYPE_UPS,
        DKP_SOURCE_TYPE_MOUSE,
        DKP_SOURCE_TYPE_KEYBOARD,
        DKP_SOURCE_TYPE_PDA,
        DKP_SOURCE_TYPE_PHONE,
        DKP_SOURCE_TYPE_UNKNOWN
} DkpSourceType;

typedef enum {
        DKP_SOURCE_STATE_CHARGING,
        DKP_SOURCE_STATE_DISCHARGING,
        DKP_SOURCE_STATE_EMPTY,
        DKP_SOURCE_STATE_FULLY_CHARGED,
        DKP_SOURCE_STATE_UNKNOWN
} DkpSourceState;

typedef enum {
        DKP_SOURCE_TECHNOLGY_LITHIUM_ION,
        DKP_SOURCE_TECHNOLGY_LITHIUM_POLYMER,
        DKP_SOURCE_TECHNOLGY_LITHIUM_IRON_PHOSPHATE,
        DKP_SOURCE_TECHNOLGY_LEAD_ACID,
        DKP_SOURCE_TECHNOLGY_NICKEL_CADMIUM,
        DKP_SOURCE_TECHNOLGY_NICKEL_METAL_HYDRIDE,
        DKP_SOURCE_TECHNOLGY_UNKNOWN
} DkpSourceTechnology;

const char		*dkp_source_type_to_text		(DkpSourceType type_enum);
const char		*dkp_source_state_to_text		(DkpSourceState state_enum);
const char		*dkp_source_technology_to_text	(DkpSourceTechnology technology_enum);
DkpSourceTechnology	 dkp_acpi_to_source_technology	(const char *type);

G_END_DECLS

#endif /* __DKP_ENUM_H__ */

