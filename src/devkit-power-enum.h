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

#ifndef __DEVKIT_POWER_ENUM_H__
#define __DEVKIT_POWER_ENUM_H__

#include <glib.h>

G_BEGIN_DECLS

typedef enum {
        DEVKIT_POWER_TYPE_LINE_POWER,
        DEVKIT_POWER_TYPE_BATTERY,
        DEVKIT_POWER_TYPE_UPS,
        DEVKIT_POWER_TYPE_MOUSE,
        DEVKIT_POWER_TYPE_KEYBOARD,
        DEVKIT_POWER_TYPE_PDA,
        DEVKIT_POWER_TYPE_PHONE,
        DEVKIT_POWER_TYPE_UNKNOWN
} DevkitPowerType;

typedef enum {
        DEVKIT_POWER_STATE_CHARGING,
        DEVKIT_POWER_STATE_DISCHARGING,
        DEVKIT_POWER_STATE_EMPTY,
        DEVKIT_POWER_STATE_FULLY_CHARGED,
        DEVKIT_POWER_STATE_UNKNOWN
} DevkitPowerState;

typedef enum {
        DEVKIT_POWER_TECHNOLGY_LITHIUM_ION,
        DEVKIT_POWER_TECHNOLGY_LITHIUM_POLYMER,
        DEVKIT_POWER_TECHNOLGY_LITHIUM_IRON_PHOSPHATE,
        DEVKIT_POWER_TECHNOLGY_LEAD_ACID,
        DEVKIT_POWER_TECHNOLGY_NICKEL_CADMIUM,
        DEVKIT_POWER_TECHNOLGY_NICKEL_METAL_HYDRIDE,
        DEVKIT_POWER_TECHNOLGY_UNKNOWN
} DevkitPowerTechnology;

const char		*devkit_power_convert_type_to_text		(DevkitPowerType type_enum);
const char		*devkit_power_convert_state_to_text		(DevkitPowerState state_enum);
const char		*devkit_power_convert_technology_to_text	(DevkitPowerTechnology technology_enum);
DevkitPowerTechnology	 devkit_power_convert_acpi_technology_to_enum	(const char *type);

G_END_DECLS

#endif /* __DEVKIT_POWER_ENUM_H__ */

