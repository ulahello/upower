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
#include <string.h>
#include "dkp-debug.h"
#include "dkp-enum.h"

/**
 * dkp_device_type_to_text:
 **/
const gchar *
dkp_device_type_to_text (DkpDeviceType type_enum)
{
	const gchar *type = NULL;
	switch (type_enum) {
	case DKP_DEVICE_TYPE_LINE_POWER:
		type = "line-power";
		break;
	case DKP_DEVICE_TYPE_BATTERY:
		type = "battery";
		break;
	case DKP_DEVICE_TYPE_UPS:
		type = "ups";
		break;
	case DKP_DEVICE_TYPE_MOUSE:
		type = "mouse";
		break;
	case DKP_DEVICE_TYPE_KEYBOARD:
		type = "keyboard";
		break;
	case DKP_DEVICE_TYPE_PDA:
		type = "pda";
		break;
	case DKP_DEVICE_TYPE_PHONE:
		type = "phone";
		break;
	case DKP_DEVICE_TYPE_UNKNOWN:
		type = "unknown";
		break;
	default:
		g_assert_not_reached ();
		break;
	}
	return type;
}

/**
 * dkp_device_type_from_text:
 **/
DkpDeviceType
dkp_device_type_from_text (const gchar *type)
{
	if (type == NULL)
		return DKP_DEVICE_TYPE_UNKNOWN;
	if (strcmp (type, "line-power") == 0)
		return DKP_DEVICE_TYPE_LINE_POWER;
	if (strcmp (type, "battery") == 0)
		return DKP_DEVICE_TYPE_BATTERY;
	if (strcmp (type, "ups") == 0)
		return DKP_DEVICE_TYPE_UPS;
	if (strcmp (type, "mouse") == 0)
		return DKP_DEVICE_TYPE_MOUSE;
	if (strcmp (type, "keyboard") == 0)
		return DKP_DEVICE_TYPE_KEYBOARD;
	if (strcmp (type, "pda") == 0)
		return DKP_DEVICE_TYPE_PDA;
	if (strcmp (type, "phone") == 0)
		return DKP_DEVICE_TYPE_PHONE;
	return DKP_DEVICE_TYPE_UNKNOWN;
}

/**
 * dkp_device_state_to_text:
 **/
const gchar *
dkp_device_state_to_text (DkpDeviceState state_enum)
{
	const gchar *state = NULL;
	switch (state_enum) {
	case DKP_DEVICE_STATE_CHARGING:
		state = "charging";
		break;
	case DKP_DEVICE_STATE_DISCHARGING:
		state = "discharging";
		break;
	case DKP_DEVICE_STATE_EMPTY:
		state = "empty";
		break;
	case DKP_DEVICE_STATE_FULLY_CHARGED:
		state = "fully-charged";
		break;
	case DKP_DEVICE_STATE_UNKNOWN:
		state = "unknown";
		break;
	default:
		g_assert_not_reached ();
		break;
	}
	return state;
}

/**
 * dkp_device_state_from_text:
 **/
DkpDeviceState
dkp_device_state_from_text (const gchar *state)
{
	if (state == NULL)
		return DKP_DEVICE_STATE_UNKNOWN;
	if (strcmp (state, "charging") == 0)
		return DKP_DEVICE_STATE_CHARGING;
	if (strcmp (state, "discharging") == 0)
		return DKP_DEVICE_STATE_DISCHARGING;
	if (strcmp (state, "empty") == 0)
		return DKP_DEVICE_STATE_EMPTY;
	if (strcmp (state, "fully-charged") == 0)
		return DKP_DEVICE_STATE_FULLY_CHARGED;
	return DKP_DEVICE_STATE_UNKNOWN;
}

/**
 * dkp_device_technology_to_text:
 **/
const gchar *
dkp_device_technology_to_text (DkpDeviceTechnology technology_enum)
{
	const gchar *technology = NULL;
	switch (technology_enum) {
	case DKP_DEVICE_TECHNOLGY_LITHIUM_ION:
		technology = "lithium-ion";
		break;
	case DKP_DEVICE_TECHNOLGY_LITHIUM_POLYMER:
		technology = "lithium-polymer";
		break;
	case DKP_DEVICE_TECHNOLGY_LITHIUM_IRON_PHOSPHATE:
		technology = "lithium-iron-phosphate";
		break;
	case DKP_DEVICE_TECHNOLGY_LEAD_ACID:
		technology = "lead-acid";
		break;
	case DKP_DEVICE_TECHNOLGY_NICKEL_CADMIUM:
		technology = "nickel-cadmium";
		break;
	case DKP_DEVICE_TECHNOLGY_NICKEL_METAL_HYDRIDE:
		technology = "nickel-metal-hydride";
		break;
	case DKP_DEVICE_TECHNOLGY_UNKNOWN:
		technology = "unknown";
		break;
	default:
		g_assert_not_reached ();
		break;
	}
	return technology;
}

/**
 * dkp_device_technology_from_text:
 **/
DkpDeviceTechnology
dkp_device_technology_from_text (const gchar *technology)
{
	if (technology == NULL)
		return DKP_DEVICE_TECHNOLGY_UNKNOWN;
	if (strcmp (technology, "lithium-ion") == 0)
		return DKP_DEVICE_TECHNOLGY_LITHIUM_ION;
	if (strcmp (technology, "lithium-polymer") == 0)
		return DKP_DEVICE_TECHNOLGY_LITHIUM_POLYMER;
	if (strcmp (technology, "lithium-iron-phosphate") == 0)
		return DKP_DEVICE_TECHNOLGY_LITHIUM_IRON_PHOSPHATE;
	if (strcmp (technology, "lead-acid") == 0)
		return DKP_DEVICE_TECHNOLGY_LEAD_ACID;
	if (strcmp (technology, "nickel-cadmium") == 0)
		return DKP_DEVICE_TECHNOLGY_NICKEL_CADMIUM;
	if (strcmp (technology, "nickel-metal-hydride") == 0)
		return DKP_DEVICE_TECHNOLGY_NICKEL_METAL_HYDRIDE;
	return DKP_DEVICE_TECHNOLGY_UNKNOWN;
}

/**
 * dkp_acpi_to_device_technology:
 **/
DkpDeviceTechnology
dkp_acpi_to_device_technology (const gchar *type)
{
	if (type == NULL) {
		return DKP_DEVICE_TECHNOLGY_UNKNOWN;
	}
	/* every case combination of Li-Ion is commonly used.. */
	if (strcasecmp (type, "li-ion") == 0 ||
	    strcasecmp (type, "lion") == 0) {
		return DKP_DEVICE_TECHNOLGY_LITHIUM_ION;
	}
	if (strcasecmp (type, "pb") == 0 ||
	    strcasecmp (type, "pbac") == 0) {
		return DKP_DEVICE_TECHNOLGY_LEAD_ACID;
	}
	if (strcasecmp (type, "lip") == 0 ||
	    strcasecmp (type, "lipo") == 0) {
		return DKP_DEVICE_TECHNOLGY_LITHIUM_POLYMER;
	}
	if (strcasecmp (type, "nimh") == 0) {
		return DKP_DEVICE_TECHNOLGY_NICKEL_METAL_HYDRIDE;
	}
	if (strcasecmp (type, "lifo") == 0) {
		return DKP_DEVICE_TECHNOLGY_LITHIUM_IRON_PHOSPHATE;
	}
	return DKP_DEVICE_TECHNOLGY_UNKNOWN;
}

