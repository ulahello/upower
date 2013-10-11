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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/**
 * SECTION:up-types
 * @short_description: Types used by UPower and libupower-glib
 *
 * These helper functions provide a way to marshal enumerated values to
 * text and back again.
 *
 * See also: #UpClient, #UpDevice
 */

#include "config.h"

#include <glib.h>

#include "up-types.h"

/**
 * up_device_kind_to_string:
 *
 * Converts a #UpDeviceKind to a string.
 *
 * Return value: identifier string
 *
 * Since: 0.9.0
 **/
const gchar *
up_device_kind_to_string (UpDeviceKind type_enum)
{
	switch (type_enum) {
	case UP_DEVICE_KIND_LINE_POWER:
		return "line-power";
	case UP_DEVICE_KIND_BATTERY:
		return "battery";
	case UP_DEVICE_KIND_UPS:
		return "ups";
	case UP_DEVICE_KIND_MONITOR:
		return "monitor";
	case UP_DEVICE_KIND_MOUSE:
		return "mouse";
	case UP_DEVICE_KIND_KEYBOARD:
		return "keyboard";
	case UP_DEVICE_KIND_PDA:
		return "pda";
	case UP_DEVICE_KIND_PHONE:
		return "phone";
	case UP_DEVICE_KIND_MEDIA_PLAYER:
		return "media-player";
	case UP_DEVICE_KIND_TABLET:
		return "tablet";
	case UP_DEVICE_KIND_COMPUTER:
		return "computer";
	default:
		return "unknown";
	}
	g_assert_not_reached ();
}

/**
 * up_device_kind_from_string:
 *
 * Converts a string to a #UpDeviceKind.
 *
 * Return value: enumerated value
 *
 * Since: 0.9.0
 **/
UpDeviceKind
up_device_kind_from_string (const gchar *type)
{
	if (type == NULL)
		return UP_DEVICE_KIND_UNKNOWN;
	if (g_strcmp0 (type, "line-power") == 0)
		return UP_DEVICE_KIND_LINE_POWER;
	if (g_strcmp0 (type, "battery") == 0)
		return UP_DEVICE_KIND_BATTERY;
	if (g_strcmp0 (type, "ups") == 0)
		return UP_DEVICE_KIND_UPS;
	if (g_strcmp0 (type, "monitor") == 0)
		return UP_DEVICE_KIND_MONITOR;
	if (g_strcmp0 (type, "mouse") == 0)
		return UP_DEVICE_KIND_MOUSE;
	if (g_strcmp0 (type, "keyboard") == 0)
		return UP_DEVICE_KIND_KEYBOARD;
	if (g_strcmp0 (type, "pda") == 0)
		return UP_DEVICE_KIND_PDA;
	if (g_strcmp0 (type, "phone") == 0)
		return UP_DEVICE_KIND_PHONE;
	if (g_strcmp0 (type, "media-player") == 0)
		return UP_DEVICE_KIND_MEDIA_PLAYER;
	if (g_strcmp0 (type, "tablet") == 0)
		return UP_DEVICE_KIND_TABLET;
	return UP_DEVICE_KIND_UNKNOWN;
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
	switch (state_enum) {
	case UP_DEVICE_STATE_CHARGING:
		return "charging";
	case UP_DEVICE_STATE_DISCHARGING:
		return "discharging";
	case UP_DEVICE_STATE_EMPTY:
		return "empty";
	case UP_DEVICE_STATE_FULLY_CHARGED:
		return "fully-charged";
	case UP_DEVICE_STATE_PENDING_CHARGE:
		return "pending-charge";
	case UP_DEVICE_STATE_PENDING_DISCHARGE:
		return "pending-discharge";
	default:
		return "unknown";
	}
	g_assert_not_reached ();
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
	switch (technology_enum) {
	case UP_DEVICE_TECHNOLOGY_LITHIUM_ION:
		return "lithium-ion";
	case UP_DEVICE_TECHNOLOGY_LITHIUM_POLYMER:
		return "lithium-polymer";
	case UP_DEVICE_TECHNOLOGY_LITHIUM_IRON_PHOSPHATE:
		return "lithium-iron-phosphate";
	case UP_DEVICE_TECHNOLOGY_LEAD_ACID:
		return "lead-acid";
	case UP_DEVICE_TECHNOLOGY_NICKEL_CADMIUM:
		return "nickel-cadmium";
	case UP_DEVICE_TECHNOLOGY_NICKEL_METAL_HYDRIDE:
		return "nickel-metal-hydride";
	default:
		return "unknown";
	}
	g_assert_not_reached ();
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
 * up_qos_kind_to_string:
 *
 * Converts a #UpQosKind to a string.
 *
 * Return value: identifier string
 *
 * Since: 0.9.0
 **/
const gchar *
up_qos_kind_to_string (UpQosKind type)
{
	if (type == UP_QOS_KIND_NETWORK)
		return "network";
	if (type == UP_QOS_KIND_CPU_DMA)
		return "cpu_dma";
	return NULL;
}

/**
 * up_qos_kind_from_string:
 *
 * Converts a string to a #UpQosKind.
 *
 * Return value: enumerated value
 *
 * Since: 0.9.0
 **/
UpQosKind
up_qos_kind_from_string (const gchar *type)
{
	if (g_strcmp0 (type, "network") == 0)
		return UP_QOS_KIND_NETWORK;
	if (g_strcmp0 (type, "cpu_dma") == 0)
		return UP_QOS_KIND_CPU_DMA;
	return UP_QOS_KIND_UNKNOWN;
}

/**
 * up_sleep_kind_to_string:
 *
 * Converts a #UpSleepKind to a string.
 *
 * Return value: identifier string
 *
 * Since: 0.9.10
 **/
const gchar *
up_sleep_kind_to_string (UpSleepKind sleep_kind_enum)
{
	const gchar *sleep_kind = NULL;
	switch (sleep_kind_enum) {
	case UP_SLEEP_KIND_SUSPEND:
		sleep_kind = "suspend";
		break;
	case UP_SLEEP_KIND_HIBERNATE:
		sleep_kind = "hibernate";
		break;
	case UP_SLEEP_KIND_HYBRID:
		sleep_kind = "hybrid";
		break;
	default:
		sleep_kind = "unknown";
		break;
	}
	return sleep_kind;
}

/**
 * up_sleep_kind_from_string:
 *
 * Converts a string to a #UpSleepKind.
 *
 * Return value: enumerated value
 *
 * Since: 0.9.10
 **/
UpSleepKind
up_sleep_kind_from_string (const gchar *sleep_kind)
{
	if (sleep_kind == NULL)
		return UP_SLEEP_KIND_UNKNOWN;
	if (g_strcmp0 (sleep_kind, "suspend") == 0)
		return UP_SLEEP_KIND_SUSPEND;
	if (g_strcmp0 (sleep_kind, "hibernate") == 0)
		return UP_SLEEP_KIND_HIBERNATE;
	if (g_strcmp0 (sleep_kind, "hybrid") == 0)
		return UP_SLEEP_KIND_HYBRID;
	return UP_SLEEP_KIND_UNKNOWN;
}
