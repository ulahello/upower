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

#include <glib.h>
#include "devkit-power-enum.h"

const char *
devkit_power_convert_type_to_text (DevkitPowerType type_enum)
{
        const char *type = NULL;
        switch (type_enum) {
        case DEVKIT_POWER_TYPE_LINE_POWER:
                type = "line-power";
                break;
       case DEVKIT_POWER_TYPE_BATTERY:
                type = "battery";
                break;
       case DEVKIT_POWER_TYPE_UPS:
                type = "ups";
                break;
       case DEVKIT_POWER_TYPE_MOUSE:
                type = "mouse";
                break;
       case DEVKIT_POWER_TYPE_KEYBOARD:
                type = "keyboard";
                break;
       case DEVKIT_POWER_TYPE_PDA:
                type = "pda";
                break;
       case DEVKIT_POWER_TYPE_PHONE:
                type = "phone";
                break;
        default:
                g_assert_not_reached ();
                break;
        }
        return type;
}

const char *
devkit_power_convert_state_to_text (DevkitPowerState state_enum)
{
        const char *state = NULL;
        switch (state_enum) {
        case DEVKIT_POWER_STATE_CHARGING:
                state = "charging";
                break;
       case DEVKIT_POWER_STATE_DISCHARGING:
                state = "discharging";
                break;
       case DEVKIT_POWER_STATE_EMPTY:
                state = "empty";
                break;
       case DEVKIT_POWER_STATE_FULLY_CHARGED:
                state = "fully-charged";
                break;
        default:
                g_assert_not_reached ();
                break;
        }
        return state;
}
