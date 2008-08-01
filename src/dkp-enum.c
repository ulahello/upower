/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
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

#include <glib.h>
#include <strings.h>
#include "dkp-enum.h"

/**
 * dkp_source_type_to_text:
 **/
const char *
dkp_source_type_to_text (DkpSourceType type_enum)
{
	const char *type = NULL;
	switch (type_enum) {
	case DKP_SOURCE_TYPE_LINE_POWER:
		type = "line-power";
		break;
	case DKP_SOURCE_TYPE_BATTERY:
		type = "battery";
		break;
	case DKP_SOURCE_TYPE_UPS:
		type = "ups";
		break;
	case DKP_SOURCE_TYPE_MOUSE:
		type = "mouse";
		break;
	case DKP_SOURCE_TYPE_KEYBOARD:
		type = "keyboard";
		break;
	case DKP_SOURCE_TYPE_PDA:
		type = "pda";
		break;
	case DKP_SOURCE_TYPE_PHONE:
		type = "phone";
		break;
	default:
		g_assert_not_reached ();
		break;
	}
	return type;
}

/**
 * dkp_source_state_to_text:
 **/
const char *
dkp_source_state_to_text (DkpSourceState state_enum)
{
	const char *state = NULL;
	switch (state_enum) {
	case DKP_SOURCE_STATE_CHARGING:
		state = "charging";
		break;
	case DKP_SOURCE_STATE_DISCHARGING:
		state = "discharging";
		break;
	case DKP_SOURCE_STATE_EMPTY:
		state = "empty";
		break;
	case DKP_SOURCE_STATE_FULLY_CHARGED:
		state = "fully-charged";
		break;
	default:
		g_assert_not_reached ();
		break;
	}
	return state;
}

/**
 * dkp_source_technology_to_text:
 **/
const char *
dkp_source_technology_to_text (DkpSourceTechnology technology_enum)
{
	const char *technology = NULL;
	switch (technology_enum) {
	case DKP_SOURCE_TECHNOLGY_LITHIUM_ION:
		technology = "lithium-ion";
		break;
	case DKP_SOURCE_TECHNOLGY_LITHIUM_POLYMER:
		technology = "lithium-polymer";
		break;
	case DKP_SOURCE_TECHNOLGY_LITHIUM_IRON_PHOSPHATE:
		technology = "lithium-iron-phosphate";
		break;
	case DKP_SOURCE_TECHNOLGY_LEAD_ACID:
		technology = "lead-acid";
		break;
	case DKP_SOURCE_TECHNOLGY_NICKEL_CADMIUM:
		technology = "nickel-cadmium";
		break;
	case DKP_SOURCE_TECHNOLGY_NICKEL_METAL_HYDRIDE:
		technology = "nickel-metal-hydride";
		break;
	case DKP_SOURCE_TECHNOLGY_UNKNOWN:
		technology = "unknown";
		break;
	default:
		g_assert_not_reached ();
		break;
	}
	return technology;
}

/**
 * dkp_acpi_to_source_technology:
 **/
DkpSourceTechnology
dkp_acpi_to_source_technology (const char *type)
{
	if (type == NULL) {
		return DKP_SOURCE_TECHNOLGY_UNKNOWN;
	}
	/* every case combination of Li-Ion is commonly used.. */
	if (strcasecmp (type, "li-ion") == 0 ||
	    strcasecmp (type, "lion") == 0) {
		return DKP_SOURCE_TECHNOLGY_LITHIUM_ION;
	}
	if (strcasecmp (type, "pb") == 0 ||
	    strcasecmp (type, "pbac") == 0) {
		return DKP_SOURCE_TECHNOLGY_LEAD_ACID;
	}
	if (strcasecmp (type, "lip") == 0 ||
	    strcasecmp (type, "lipo") == 0) {
		return DKP_SOURCE_TECHNOLGY_LITHIUM_POLYMER;
	}
	if (strcasecmp (type, "nimh") == 0) {
		return DKP_SOURCE_TECHNOLGY_NICKEL_METAL_HYDRIDE;
	}
	if (strcasecmp (type, "lifo") == 0) {
		return DKP_SOURCE_TECHNOLGY_LITHIUM_IRON_PHOSPHATE;
	}
	return DKP_SOURCE_TECHNOLGY_UNKNOWN;
}

