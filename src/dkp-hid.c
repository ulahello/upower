/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2006-2008 Richard Hughes <richard@hughsie.com>
 *
 * Based on hid-ups.c: Copyright (c) 2001 Vojtech Pavlik <vojtech@ucw.cz>
 *                     Copyright (c) 2001 Paul Stewart <hiddev@wetlogic.net>
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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <string.h>
#include <math.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n-lib.h>
#include <glib-object.h>
#include <devkit-gobject.h>

/* asm/types.h required for __s32 in linux/hiddev.h */
#include <asm/types.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/hiddev.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "sysfs-utils.h"
#include "dkp-debug.h"
#include "dkp-enum.h"
#include "dkp-object.h"
#include "dkp-hid.h"

#define DKP_HID_REFRESH_TIMEOUT			30l

#define DKP_HID_USAGE				0x840000
#define DKP_HID_SERIAL				0x8400fe
#define DKP_HID_CHEMISTRY			0x850089
#define DKP_HID_CAPACITY_MODE			0x85002c
#define DKP_HID_BATTERY_VOLTAGE			0x840030
#define DKP_HID_BELOW_RCL			0x840042
#define DKP_HID_SHUTDOWN_IMMINENT		0x840069
#define DKP_HID_PRODUCT				0x8400fe
#define DKP_HID_SERIAL_NUMBER			0x8400ff
#define DKP_HID_CHARGING			0x850044
#define DKP_HID_DISCHARGING 			0x850045
#define DKP_HID_REMAINING_CAPACITY		0x850066
#define DKP_HID_RUNTIME_TO_EMPTY		0x850068
#define DKP_HID_AC_PRESENT			0x8500d0
#define DKP_HID_BATTERY_PRESENT			0x8500d1
#define DKP_HID_DESIGN_CAPACITY			0x850083
#define DKP_HID_DEVICE_NAME			0x850088
#define DKP_HID_DEVICE_CHEMISTRY		0x850089
#define DKP_HID_RECHARGEABLE			0x85008b
#define DKP_HID_OEM_INFORMATION			0x85008f

#define DKP_HID_PAGE_GENERIC_DESKTOP		0x01
#define DKP_HID_PAGE_CONSUMER_PRODUCT		0x0c
#define DKP_HID_PAGE_USB_MONITOR		0x80
#define DKP_HID_PAGE_USB_ENUMERATED_VALUES	0x81
#define DKP_HID_PAGE_VESA_VIRTUAL_CONTROLS	0x82
#define DKP_HID_PAGE_RESERVED_MONITOR		0x83
#define DKP_HID_PAGE_POWER_DEVICE		0x84
#define DKP_HID_PAGE_BATTERY_SYSTEM		0x85

struct DkpHidPrivate
{
	guint			 poll_timer_id;
	int			 fd;
};

static void	dkp_hid_class_init	(DkpHidClass	*klass);

G_DEFINE_TYPE (DkpHid, dkp_hid, DKP_TYPE_DEVICE)
#define DKP_HID_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), DKP_TYPE_HID, DkpHidPrivate))

static gboolean		 dkp_hid_refresh	 	(DkpDevice *device);

/**
 * dkp_hid_is_ups:
 **/
static gboolean
dkp_hid_is_ups (DkpHid *hid)
{
	guint i;
	int retval;
	gboolean ret = FALSE;
	struct hiddev_devinfo device_info;

	/* get device info */
	retval = ioctl (hid->priv->fd, HIDIOCGDEVINFO, &device_info);
	if (retval < 0) {
		dkp_debug ("HIDIOCGDEVINFO failed: %s", strerror (errno));
		goto out;
	}

	/* can we use the hid device as a UPS? */
	for (i = 0; i < device_info.num_applications; i++) {
		retval = ioctl (hid->priv->fd, HIDIOCAPPLICATION, i);
		if (retval >> 16 == DKP_HID_PAGE_POWER_DEVICE) {
			ret = TRUE;
			goto out;
		}
	}
out:
	return ret;
}

/**
 * dkp_hid_poll:
 **/
static gboolean
dkp_hid_poll (DkpHid *hid)
{
	gboolean ret;
	DkpDevice *device = DKP_DEVICE (hid);
	DkpObject *obj = dkp_device_get_obj (device);

	dkp_debug ("Polling: %s", obj->native_path);
	ret = dkp_hid_refresh (device);
	if (ret)
		dkp_device_emit_changed (device);
	return TRUE;
}

/**
 * dkp_hid_get_string:
 **/
static const gchar *
dkp_hid_get_string (DkpHid *hid, int sindex)
{
	static struct hiddev_string_descriptor sdesc;

	/* nothing to get */
	if (sindex == 0)
		return "";

	sdesc.index = sindex;

	/* failed */
	if (ioctl (hid->priv->fd, HIDIOCGSTRING, &sdesc) < 0)
		return "";

	dkp_debug ("value: '%s'", sdesc.value);
	return sdesc.value;
}

/**
 * dkp_hid_set_obj:
 **/
static gboolean
dkp_hid_set_obj (DkpHid *hid, int code, int value)
{
	const gchar *type;
	gboolean ret = TRUE;
	DkpDevice *device = DKP_DEVICE (hid);
	DkpObject *obj = dkp_device_get_obj (device);

	switch (code) {
	case DKP_HID_REMAINING_CAPACITY:
		obj->battery_percentage = value;
		break;
	case DKP_HID_RUNTIME_TO_EMPTY:
		obj->battery_time_to_empty = value;
		break;
	case DKP_HID_CHARGING:
		if (value != 0)
			obj->battery_state = DKP_DEVICE_STATE_CHARGING;
		break;
	case DKP_HID_DISCHARGING:
		if (value != 0)
			obj->battery_state = DKP_DEVICE_STATE_DISCHARGING;
		break;
	case DKP_HID_BATTERY_PRESENT:
		obj->battery_is_present = (value != 0);
		break;
	case DKP_HID_DEVICE_NAME:
		//obj->device_name = dkp_hid_get_string (hid, value);
		break;
	case DKP_HID_CHEMISTRY:
		type = dkp_hid_get_string (hid, value);
		obj->battery_technology = dkp_acpi_to_device_technology (type);
		break;
	case DKP_HID_RECHARGEABLE:
		obj->battery_is_rechargeable = (value != 0);
		break;
	case DKP_HID_OEM_INFORMATION:
		obj->vendor = g_strdup (dkp_hid_get_string (hid, value));
		break;
	case DKP_HID_PRODUCT:
		obj->model = g_strdup (dkp_hid_get_string (hid, value));
		break;
	case DKP_HID_SERIAL_NUMBER:
		obj->serial = g_strdup (dkp_hid_get_string (hid, value));
		break;
	case DKP_HID_DESIGN_CAPACITY:
		obj->battery_energy_full_design = value;
		break;
	default:
		ret = FALSE;
		break;
	}
	return ret;
}

/**
 * dkp_hid_get_all_data:
 **/
static gboolean
dkp_hid_get_all_data (DkpHid *hid)
{
	struct hiddev_report_info rinfo;
	struct hiddev_field_info finfo;
	struct hiddev_usage_ref uref;
	int rtype;
	guint i, j;

	/* get all results */
	for (rtype = HID_REPORT_TYPE_MIN; rtype <= HID_REPORT_TYPE_MAX; rtype++) {
		rinfo.report_type = rtype;
		rinfo.report_id = HID_REPORT_ID_FIRST;
		while (ioctl (hid->priv->fd, HIDIOCGREPORTINFO, &rinfo) >= 0) {
			for (i = 0; i < rinfo.num_fields; i++) { 
				memset (&finfo, 0, sizeof (finfo));
				finfo.report_type = rinfo.report_type;
				finfo.report_id = rinfo.report_id;
				finfo.field_index = i;
				ioctl (hid->priv->fd, HIDIOCGFIELDINFO, &finfo);

				memset (&uref, 0, sizeof (uref));
				for (j = 0; j < finfo.maxusage; j++) {
					uref.report_type = finfo.report_type;
					uref.report_id = finfo.report_id;
					uref.field_index = i;
					uref.usage_index = j;
					ioctl (hid->priv->fd, HIDIOCGUCODE, &uref);
					ioctl (hid->priv->fd, HIDIOCGUSAGE, &uref);

					/* process each */
					dkp_hid_set_obj (hid, uref.usage_code, uref.value);
				}
			}
			rinfo.report_id |= HID_REPORT_ID_NEXT;
		}
	}
	return TRUE;
}

/**
 * dkp_hid_coldplug:
 **/
static gboolean
dkp_hid_coldplug (DkpDevice *device)
{
	DkpHid *hid = DKP_HID (device);
	DevkitDevice *d;
	gboolean ret = FALSE;
	const gchar *device_file;
	const gchar *type;
	DkpObject *obj = dkp_device_get_obj (device);

	/* detect what kind of device we are */
	d = dkp_device_get_d (device);
	if (d == NULL)
		dkp_error ("could not get device");

	/* get the type */
	type = devkit_device_get_property (d, "ID_BATTERY_TYPE");
	if (type == NULL || strcmp (type, "ups") != 0) {
		dkp_debug ("not a UPS device");
		goto out;
	}

	/* get the device file */
	device_file = devkit_device_get_device_file (d);
	if (device_file == NULL) {
		dkp_debug ("could not get device file for HID device");
		goto out;
	}

	/* connect to the device */
	hid->priv->fd = open (device_file, O_RDONLY | O_NONBLOCK);
	if (hid->priv->fd < 0) {
		dkp_debug ("cannot open device file %s", device_file);
		goto out;
	}

	/* first check that we are an UPS */
	ret = dkp_hid_is_ups (hid);
	if (!ret) {
		dkp_debug ("not a HID device: %s", device_file);
		goto out;
	}

	/* hardcode some values */
	obj->type = DKP_DEVICE_TYPE_UPS;
	obj->battery_is_rechargeable = TRUE;
	obj->power_supply = TRUE;
	obj->battery_is_present = TRUE;

	/* try and get from udev if UPS is being difficult */
	if (obj->vendor == NULL)
		obj->vendor = g_strdup (devkit_device_get_property (d, "ID_VENDOR"));

	/* coldplug everything */
	dkp_hid_get_all_data (hid);

	/* coldplug */
	ret = dkp_hid_refresh (device);

out:
	return ret;
}

/**
 * dkp_hid_refresh:
 **/
static gboolean
dkp_hid_refresh (DkpDevice *device)
{
	gboolean set = FALSE;
	gboolean ret = FALSE;
	GTimeVal time;
	guint i;
	struct hiddev_event ev[64];
	int rd;
	DkpHid *hid = DKP_HID (device);
	DkpObject *obj = dkp_device_get_obj (device);

	/* reset time */
	g_get_current_time (&time);
	obj->update_time = time.tv_sec;

	/* read any data -- it's okay if there's nothing as we are non-blocking */
	rd = read (hid->priv->fd, ev, sizeof (ev));
	if (rd < (int) sizeof (ev[0])) {
		ret = TRUE;
		goto out;
	}

	/* process each event */
	for (i=0; i < rd / sizeof (ev[0]); i++) {
		set = dkp_hid_set_obj (hid, ev[i].hid, ev[i].value);

		/* if only takes one match to make refresh a success */
		if (set)
			ret = TRUE;
	}
out:
	return ret;
}

/**
 * dkp_hid_init:
 **/
static void
dkp_hid_init (DkpHid *hid)
{
	hid->priv = DKP_HID_GET_PRIVATE (hid);
	hid->priv->fd = -1;
	hid->priv->poll_timer_id = g_timeout_add_seconds (DKP_HID_REFRESH_TIMEOUT,
							  (GSourceFunc) dkp_hid_poll, hid);
}

/**
 * dkp_hid_finalize:
 **/
static void
dkp_hid_finalize (GObject *object)
{
	DkpHid *hid;

	g_return_if_fail (object != NULL);
	g_return_if_fail (DKP_IS_HID (object));

	hid = DKP_HID (object);
	g_return_if_fail (hid->priv != NULL);

	if (hid->priv->fd > 0)
		close (hid->priv->fd);
	if (hid->priv->poll_timer_id > 0)
		g_source_remove (hid->priv->poll_timer_id);

	G_OBJECT_CLASS (dkp_hid_parent_class)->finalize (object);
}

/**
 * dkp_hid_class_init:
 **/
static void
dkp_hid_class_init (DkpHidClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	DkpDeviceClass *device_class = DKP_DEVICE_CLASS (klass);

	object_class->finalize = dkp_hid_finalize;
	device_class->coldplug = dkp_hid_coldplug;
	device_class->refresh = dkp_hid_refresh;

	g_type_class_add_private (klass, sizeof (DkpHidPrivate));
}

/**
 * dkp_hid_new:
 **/
DkpHid *
dkp_hid_new (void)
{
	return g_object_new (DKP_TYPE_HID, NULL);
}

