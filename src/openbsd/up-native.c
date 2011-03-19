/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011 Landry Breuil <landry@openbsd.org>
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

#include "up-apm-native.h"
#include "up-native.h"

/* XXX why does this macro needs to be in the .c ? */
G_DEFINE_TYPE (UpApmNative, up_apm_native, G_TYPE_OBJECT)

static void
up_apm_native_class_init (UpApmNativeClass *klass)
{
}

static void
up_apm_native_init (UpApmNative *self)
{
	self->path = "empty";
}

UpApmNative *
up_apm_native_new(const gchar * path)
{
	UpApmNative *native;
	native = UP_APM_NATIVE (g_object_new (UP_TYPE_APM_NATIVE, NULL));
	native->path = g_strdup(path);
	return native;
}

const gchar *
up_apm_native_get_path(UpApmNative * native)
{
	return native->path;
}

/**
 * up_native_get_native_path:
 * @object: the native tracking object
 *
 * This converts a GObject used as the device data into a native path.
 *
 * Return value: The native path for the device which is unique, e.g. "/sys/class/power/BAT1"
 **/
const gchar *
up_native_get_native_path (GObject *object)
{
	return up_apm_native_get_path (UP_APM_NATIVE (object));
}

/**
 * detect if we are on a desktop system or a laptop
 * heuristic : laptop if sysctl hw.acpiac0 is present (TODO) or if apm acstate != APM_AC_UNKNOWN
 */
gboolean
up_native_is_laptop()
{
	int apm_fd;
	struct apm_power_info bstate;
	struct sensordev acpiac;

	if (up_native_get_sensordev("acpiac0", &acpiac))
		return TRUE;

	if ((apm_fd = open("/dev/apm", O_RDONLY)) == -1) {
		if (errno != ENXIO && errno != ENOENT)
			g_error("cannot open device file");
	}
	if (-1 == ioctl(apm_fd, APM_IOC_GETPOWER, &bstate))
		g_error("ioctl on fd %d failed : %s", apm_fd, g_strerror(errno));
	close(apm_fd);
	return bstate.ac_state != APM_AC_UNKNOWN;
}

/**
 * get a sensordev by its xname (acpibatX/acpiacX)
 * returns a gboolean if found or not
 */
gboolean
up_native_get_sensordev(const char * id, struct sensordev * snsrdev)
{
	int devn;
	size_t sdlen = sizeof(struct sensordev);
	int mib[] = {CTL_HW, HW_SENSORS, 0, 0 ,0};

	for (devn = 0 ; ; devn++) {
		mib[2] = devn;
		if (sysctl(mib, 3, snsrdev, &sdlen, NULL, 0) == -1) {
			if (errno == ENXIO)
				continue;
			if (errno == ENOENT)
				break;
		}
		if (!strcmp(snsrdev->xname, id))
			return TRUE;
	}
	return FALSE;
}
