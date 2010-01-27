/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007-2009 Richard Hughes <richard@hughsie.com>
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

#include "config.h"

#include <glib-object.h>
#include "egg-test.h"
#include "egg-debug.h"

#include "up-backend.h"
#include "up-daemon.h"
#include "up-device.h"
#include "up-device-list.h"
#include "up-history.h"
#include "up-native.h"
#include "up-polkit.h"
#include "up-qos.h"
#include "up-wakeups.h"

int
main (int argc, char **argv)
{
	EggTest *test;

	g_type_init ();
	test = egg_test_init ();
	egg_debug_init (&argc, &argv);

	/* tests go here */
	up_backend_test (test);
	up_device_test (test);
	up_device_list_test (test);
	up_history_test (test);
	up_native_test (test);
	up_polkit_test (test);
	up_qos_test (test);
	up_wakeups_test (test);
	up_daemon_test (test);

	return (egg_test_finish (test));
}

