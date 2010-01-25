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

#include "config.h"

#include <glib.h>

#include "up-types.h"

/**
 * up_device_type_to_string:
 *
 * Converts a #UpDeviceType to a string.
 *
 * Return value: identifier string
 *
 * Since: 0.9.0
 **/
const gchar *
up_device_type_to_string (UpDeviceType type_enum)
{
	const gchar *type = NULL;
	switch (type_enum) {
	case UP_DEVICE_TYPE_LINE_POWER:
		type = "line-power";
		break;
	case UP_DEVICE_TYPE_BATTERY:
		type = "battery";
		break;
	case UP_DEVICE_TYPE_UPS:
		type = "ups";
		break;
	case UP_DEVICE_TYPE_MONITOR:
		type = "monitor";
		break;
	case UP_DEVICE_TYPE_MOUSE:
		type = "mouse";
		break;
	case UP_DEVICE_TYPE_KEYBOARD:
		type = "keyboard";
		break;
	case UP_DEVICE_TYPE_PDA:
		type = "pda";
		break;
	case UP_DEVICE_TYPE_PHONE:
		type = "phone";
		break;
	default:
		type = "unknown";
		break;
	}
	return type;
}

/**
 * up_device_type_from_string:
 *
 * Converts a string to a #UpDeviceType.
 *
 * Return value: enumerated value
 *
 * Since: 0.9.0
 **/
UpDeviceType
up_device_type_from_string (const gchar *type)
{
	if (type == NULL)
		return UP_DEVICE_TYPE_UNKNOWN;
	if (g_strcmp0 (type, "line-power") == 0)
		return UP_DEVICE_TYPE_LINE_POWER;
	if (g_strcmp0 (type, "battery") == 0)
		return UP_DEVICE_TYPE_BATTERY;
	if (g_strcmp0 (type, "ups") == 0)
		return UP_DEVICE_TYPE_UPS;
	if (g_strcmp0 (type, "monitor") == 0)
		return UP_DEVICE_TYPE_MONITOR;
	if (g_strcmp0 (type, "mouse") == 0)
		return UP_DEVICE_TYPE_MOUSE;
	if (g_strcmp0 (type, "keyboard") == 0)
		return UP_DEVICE_TYPE_KEYBOARD;
	if (g_strcmp0 (type, "pda") == 0)
		return UP_DEVICE_TYPE_PDA;
	if (g_strcmp0 (type, "phone") == 0)
		return UP_DEVICE_TYPE_PHONE;
	return UP_DEVICE_TYPE_UNKNOWN;
}

/**
 * up_device_state_to_string:
 *
 * Converts a #UpDeviceState to a string.
 *
 * Return value: identifier string
 *
 * Since: 0.9.0
 **/
const gchar *
up_device_state_to_string (UpDeviceState state_enum)
{
	const gchar *state = NULL;
	switch (state_enum) {
	case UP_DEVICE_STATE_CHARGING:
		state = "charging";
		break;
	case UP_DEVICE_STATE_DISCHARGING:
		state = "discharging";
		break;
	case UP_DEVICE_STATE_EMPTY:
		state = "empty";
		break;
	case UP_DEVICE_STATE_FULLY_CHARGED:
		state = "fully-charged";
		break;
	case UP_DEVICE_STATE_PENDING_CHARGE:
		state = "pending-charge";
		break;
	case UP_DEVICE_STATE_PENDING_DISCHARGE:
		state = "pending-discharge";
		break;
	default:
		state = "unknown";
		break;
	}
	return state;
}

/**
 * up_device_state_from_string:
 *
 * Converts a string to a #UpDeviceState.
 *
 * Return value: enumerated value
 *
 * Since: 0.9.0
 **/
UpDeviceState
up_device_state_from_string (const gchar *state)
{
	if (state == NULL)
		return UP_DEVICE_STATE_UNKNOWN;
	if (g_strcmp0 (state, "charging") == 0)
		return UP_DEVICE_STATE_CHARGING;
	if (g_strcmp0 (state, "discharging") == 0)
		return UP_DEVICE_STATE_DISCHARGING;
	if (g_strcmp0 (state, "empty") == 0)
		return UP_DEVICE_STATE_EMPTY;
	if (g_strcmp0 (state, "fully-charged") == 0)
		return UP_DEVICE_STATE_FULLY_CHARGED;
	if (g_strcmp0 (state, "pending-charge") == 0)
		return UP_DEVICE_STATE_PENDING_CHARGE;
	if (g_strcmp0 (state, "pending-discharge") == 0)
		return UP_DEVICE_STATE_PENDING_DISCHARGE;
	return UP_DEVICE_STATE_UNKNOWN;
}

/**
 * up_device_technology_to_string:
 *
 * Converts a #UpDeviceTechnology to a string.
 *
 * Return value: identifier string
 *
 * Since: 0.9.0
 **/
const gchar *
up_device_technology_to_string (UpDeviceTechnology technology_enum)
{
	const gchar *technology = NULL;
	switch (technology_enum) {
	case UP_DEVICE_TECHNOLOGY_LITHIUM_ION:
		technology = "lithium-ion";
		break;
	case UP_DEVICE_TECHNOLOGY_LITHIUM_POLYMER:
		technology = "lithium-polymer";
		break;
	case UP_DEVICE_TECHNOLOGY_LITHIUM_IRON_PHOSPHATE:
		technology = "lithium-iron-phosphate";
		break;
	case UP_DEVICE_TECHNOLOGY_LEAD_ACID:
		technology = "lead-acid";
		break;
	case UP_DEVICE_TECHNOLOGY_NICKEL_CADMIUM:
		technology = "nickel-cadmium";
		break;
	case UP_DEVICE_TECHNOLOGY_NICKEL_METAL_HYDRIDE:
		technology = "nickel-metal-hydride";
		break;
	default:
		technology = "unknown";
		break;
	}
	return technology;
}

/**
 * up_device_technology_from_string:
 *
 * Converts a string to a #UpDeviceTechnology.
 *
 * Return value: enumerated value
 *
 * Since: 0.9.0
 **/
UpDeviceTechnology
up_device_technology_from_string (const gchar *technology)
{
	if (technology == NULL)
		return UP_DEVICE_TECHNOLOGY_UNKNOWN;
	if (g_strcmp0 (technology, "lithium-ion") == 0)
		return UP_DEVICE_TECHNOLOGY_LITHIUM_ION;
	if (g_strcmp0 (technology, "lithium-polymer") == 0)
		return UP_DEVICE_TECHNOLOGY_LITHIUM_POLYMER;
	if (g_strcmp0 (technology, "lithium-iron-phosphate") == 0)
		return UP_DEVICE_TECHNOLOGY_LITHIUM_IRON_PHOSPHATE;
	if (g_strcmp0 (technology, "lead-acid") == 0)
		return UP_DEVICE_TECHNOLOGY_LEAD_ACID;
	if (g_strcmp0 (technology, "nickel-cadmium") == 0)
		return UP_DEVICE_TECHNOLOGY_NICKEL_CADMIUM;
	if (g_strcmp0 (technology, "nickel-metal-hydride") == 0)
		return UP_DEVICE_TECHNOLOGY_NICKEL_METAL_HYDRIDE;
	return UP_DEVICE_TECHNOLOGY_UNKNOWN;
}

/**
 * up_qos_type_to_string:
 *
 * Converts a #UpQosType to a string.
 *
 * Return value: identifier string
 *
 * Since: 0.9.0
 **/
const gchar *
up_qos_type_to_string (UpQosType type)
{
	if (type == UP_QOS_TYPE_NETWORK)
		return "network";
	if (type == UP_QOS_TYPE_CPU_DMA)
		return "cpu_dma";
	return NULL;
}

/**
 * up_qos_type_from_string:
 *
 * Converts a string to a #UpQosType.
 *
 * Return value: enumerated value
 *
 * Since: 0.9.0
 **/
UpQosType
up_qos_type_from_string (const gchar *type)
{
	if (g_strcmp0 (type, "network") == 0)
		return UP_QOS_TYPE_NETWORK;
	if (g_strcmp0 (type, "cpu_dma") == 0)
		return UP_QOS_TYPE_CPU_DMA;
	return UP_QOS_TYPE_UNKNOWN;
}

