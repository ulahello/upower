/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012 Julien Danjou <julien@danjou.info>
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

#include <linux/hidraw.h>
#include <linux/input.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gprintf.h>
#include <glib/gi18n-lib.h>
#include <glib-object.h>
#include <gudev/gudev.h>

#include "sysfs-utils.h"
#include "up-types.h"
#include "up-device-lg-unifying.h"

/* Arbitrary value used in ping */
#define HIDPP_PING_DATA 0x42

#define HIDPP_RECEIVER_ADDRESS 0xff

#define HIDPP_RESPONSE_SHORT_LENGTH 7
#define HIDPP_RESPONSE_LONG_LENGTH 20

#define HIDPP_HEADER_REQUEST 0x10
#define HIDPP_HEADER_RESPONSE  0x11

/* HID++ 1.0 */
#define HIDPP_READ_SHORT_REGISTER                          0x81
#define HIDPP_READ_SHORT_REGISTER_BATTERY                  0x0d

#define HIDPP_READ_LONG_REGISTER                           0x83
#define HIDPP_READ_LONG_REGISTER_DEVICE_TYPE                 11
#define HIDPP_READ_LONG_REGISTER_DEVICE_TYPE_KEYBOARD       0x1
#define HIDPP_READ_LONG_REGISTER_DEVICE_TYPE_MOUSE          0x2
#define HIDPP_READ_LONG_REGISTER_DEVICE_TYPE_NUMPAD         0x3
#define HIDPP_READ_LONG_REGISTER_DEVICE_TYPE_PRESENTER      0x4
#define HIDPP_READ_LONG_REGISTER_DEVICE_TYPE_REMOTE_CONTROL 0x7
#define HIDPP_READ_LONG_REGISTER_DEVICE_TYPE_TRACKBALL      0x8
#define HIDPP_READ_LONG_REGISTER_DEVICE_TYPE_TOUCHPAD       0x9
#define HIDPP_READ_LONG_REGISTER_DEVICE_TYPE_TABLET         0xa
#define HIDPP_READ_LONG_REGISTER_DEVICE_TYPE_GAMEPAD        0xb
#define HIDPP_READ_LONG_REGISTER_DEVICE_TYPE_JOYSTICK       0xc

#define HIDPP_ERR_INVALID_SUBID                            0x8f

/* HID++ 2.0 */
#define HIDPP_FEATURE_ROOT                            0x0000
/* This is the only feature that has an hard coded index */
#define HIDPP_FEATURE_ROOT_INDEX                        0x00
#define HIDPP_FEATURE_ROOT_FUNCTION_GETFEATURE   (0x00 << 4)
#define HIDPP_FEATURE_ROOT_FUNCTION_PING         (0x01 << 4)

#define HIDPP_FEATURE_GETDEVICENAMETYPE                             0x0005
#define HIDPP_FEATURE_GETDEVICENAMETYPE_FUNCTION_GETCOUNT      (0x00 << 4)
#define HIDPP_FEATURE_GETDEVICENAMETYPE_FUNCTION_GETDEVICENAME (0x01 << 4)

/* I wish i has the spec for this, but I don't so I invented the name */
#define HIDPP_FEATURE_K750_BATTERY                                   0x4301
#define HIDPP_FEATURE_K750_BATTERY_FUNCTION_STARTLUXANDBATTERY   (0x00 << 4)
#define HIDPP_FEATURE_K750_BATTERY_FUNCTION_LUXANDBATTERYEVENT   (0x01 << 4)

#define HIDPP_FEATURE_FUNCTION_AS_ARG(feature)	\
	feature >> 8, feature, 0x00

#define USB_VENDOR_ID_LOGITECH			"046d"
#define USB_DEVICE_ID_UNIFYING_RECEIVER		"c52b"
#define USB_DEVICE_ID_UNIFYING_RECEIVER_2	"c532"

#define UP_DEVICE_UNIFYING_READ_RESPONSE_TIMEOUT      3000 /* miliseconds */
#define UP_DEVICE_UNIFYING_REFRESH_TIMEOUT		60L /* seconds */

struct UpDeviceUnifyingPrivate
{
	guint			 poll_timer_id;
	int			 fd;
	/* Device index on the Unifying "bus" */
	gint			 device_index;
	gint			 feature_k750_battery_index;
	GIOChannel		*channel;
	guint			 channel_source_id;
};

G_DEFINE_TYPE (UpDeviceUnifying, up_device_unifying, UP_TYPE_DEVICE)
#define UP_DEVICE_UNIFYING_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), UP_TYPE_DEVICE_UNIFYING, UpDeviceUnifyingPrivate))

/**
 * up_device_unifying_event_io:
 *
 * Read events from Unifying device, and treats them.
 **/
static gboolean
up_device_unifying_event_io (GIOChannel *channel, GIOCondition condition, gpointer data)
{
	guint8 buf[HIDPP_RESPONSE_LONG_LENGTH];
	UpDeviceUnifying *unifying = data;
	UpDevice *device = UP_DEVICE (unifying);
	GTimeVal timeval;
	guint16 lux;

	while (read (unifying->priv->fd, buf, sizeof(buf)) > 0)
		if (buf[0] == HIDPP_HEADER_RESPONSE &&
		    buf[1] == unifying->priv->device_index &&
		    buf[2] == unifying->priv->feature_k750_battery_index &&
		    buf[3] == HIDPP_FEATURE_K750_BATTERY_FUNCTION_LUXANDBATTERYEVENT) {
			lux = (buf[5] << 8) | buf[6];
			if (lux > 200) {
				g_object_set (device,
					      "state", UP_DEVICE_STATE_CHARGING,
					      "power-supply", TRUE,
					      NULL);
			} else if (lux > 0) {
				g_object_set (device,
					      "state", UP_DEVICE_STATE_DISCHARGING,
					      "power-supply", TRUE,
					      NULL);
			} else {
				g_object_set (device,
					      "state", UP_DEVICE_STATE_DISCHARGING,
					      "power-supply", FALSE,
					      NULL);
			}

			g_get_current_time (&timeval);

			g_object_set (device,
				      "update-time", (guint64) timeval.tv_sec,
				      "percentage", (gdouble) (guint8) buf[4],
				      "luminosity", (gdouble) lux,
				      NULL);
		}

	return TRUE;
}

static gint
up_device_unifying_read_response (int fd,
				  guint8 request[],
				  size_t count,
				  gint64 start_time)
{
	GPollFD poll[] = {
		{
			.fd = fd,
			.events = G_IO_IN | G_IO_HUP | G_IO_ERR,
		},
	};
	gint ret;

	/* If we started to wait for a particular response more than some
	 * time ago, abort */
	if (g_get_monotonic_time () - start_time
	    >= UP_DEVICE_UNIFYING_READ_RESPONSE_TIMEOUT * 1000)
		return -1;

	ret = g_poll (poll, G_N_ELEMENTS(poll),
		      UP_DEVICE_UNIFYING_READ_RESPONSE_TIMEOUT);

	if (ret > 0)
		return read (fd, request, count);

	return ret;
}

/**
 * up_device_unifying_hidpp1_set_battery:
 *
 * Send a READ SHORT REGISTER call to the device, and set battery status.
 **/
static gboolean
up_device_unifying_hidpp1_set_battery (UpDeviceUnifying *unifying)
{
	UpDevice *device = UP_DEVICE (unifying);
	guint8 request[] = {
		HIDPP_HEADER_REQUEST,
		unifying->priv->device_index,
		HIDPP_READ_SHORT_REGISTER,
		HIDPP_READ_SHORT_REGISTER_BATTERY,
		0x00, 0x00, 0x00,
	};
	guint8 buf[HIDPP_RESPONSE_LONG_LENGTH];
	gint64 start_time;

	if (write (unifying->priv->fd, request, sizeof(request)) != sizeof(request)) {
		g_debug ("Unable to read battery status from Unifying device %d",
			 unifying->priv->device_index);
		return FALSE;
	}

	start_time = g_get_monotonic_time ();

	while (up_device_unifying_read_response (unifying->priv->fd, buf, sizeof (buf), start_time) > 0)
		if (buf[0] == HIDPP_HEADER_REQUEST
		    && buf[1] == unifying->priv->device_index
		    && buf[2] == HIDPP_READ_SHORT_REGISTER
		    && buf[3] == HIDPP_READ_SHORT_REGISTER_BATTERY) {
			g_object_set (device,
				      "percentage", (gdouble) buf[4],
				      NULL);
			return TRUE;
		}

	return FALSE;
}

/**
 * up_device_unifying_hidpp2_get_feature_index:
 *
 * Get a Unifying HID++ 2.0 feature index and return it.
 * Returns 0 if the feature does not exists on this device.
 **/
static guint8
up_device_unifying_hidpp2_get_feature_index (UpDeviceUnifying *unifying, guint16 feature)
{
	guint8 buf[HIDPP_RESPONSE_LONG_LENGTH];
	guint8 request[] = {
		HIDPP_HEADER_REQUEST,
		unifying->priv->device_index,
		HIDPP_FEATURE_ROOT_INDEX,
		HIDPP_FEATURE_ROOT_FUNCTION_GETFEATURE,
		HIDPP_FEATURE_FUNCTION_AS_ARG(feature)
	};
	gint64 start_time;

	/* Request the device name feature index */
	if (write (unifying->priv->fd, request, sizeof(request)) != sizeof(request)) {
		g_debug ("Unable to send GetFeature request to device");
		return -1;
	}

	start_time = g_get_monotonic_time ();

	while (up_device_unifying_read_response (unifying->priv->fd, buf, sizeof (buf), start_time) > 0)
		if (buf[0] == HIDPP_HEADER_RESPONSE &&
		    buf[1] == unifying->priv->device_index &&
		    buf[2] == HIDPP_FEATURE_ROOT_INDEX &&
		    buf[3] == HIDPP_FEATURE_ROOT_FUNCTION_GETFEATURE)
			return buf[4];
	return -1;
}


/**
 * up_device_unifying_hidpp2_set_battery:
 *
 * Send a bunch of HID++ requests to get the device battery and set it.
 **/
static gboolean
up_device_unifying_hidpp2_set_battery (UpDeviceUnifying *unifying)
{
	guint8 request[] = {
		HIDPP_HEADER_REQUEST,
		unifying->priv->device_index,
		0x00, 0x00, 0x00, 0x00, 0x00,
	};

	if (unifying->priv->feature_k750_battery_index == -1)
		unifying->priv->feature_k750_battery_index =
			up_device_unifying_hidpp2_get_feature_index (unifying, HIDPP_FEATURE_K750_BATTERY);

	if (unifying->priv->feature_k750_battery_index == 0) {
		/* Probably not a K750 */
		/* TODO: add support for BatteryLevelStatus */
	} else {
		/* This request will make the keyboard send a bunch of packets
		 * (events) with lux-meter and battery information */
		request[2] = unifying->priv->feature_k750_battery_index;
		request[3] = HIDPP_FEATURE_K750_BATTERY_FUNCTION_STARTLUXANDBATTERY;
		/* Don't know what this means */
		request[4] = 0x78;
		request[5] = 0x01;


		if (write (unifying->priv->fd, request, sizeof(request)) != sizeof(request)) {
			g_debug ("Unable to send K750 battery/lux events start request to device");
			return FALSE;
		}

		return TRUE;
	}

	return FALSE;
}

/**
 * up_device_unifying_hidpp2_get_device_name:
 *
 * Send a bunch of HID++ requests to get the device name (model) and return
 * it.
 **/
static GString *
up_device_unifying_hidpp2_get_device_name (UpDeviceUnifying *unifying)
{
	GString *name = NULL;
	guint8 buf[HIDPP_RESPONSE_LONG_LENGTH];
	ssize_t res;
	guint8 request[] = {
		HIDPP_HEADER_REQUEST,
		unifying->priv->device_index,
		0x00,
		HIDPP_FEATURE_GETDEVICENAMETYPE_FUNCTION_GETCOUNT,
		0x00, 0x00, 0x00,
	};
	ssize_t name_length = 0;
	gint64 start_time;

	request[2] = up_device_unifying_hidpp2_get_feature_index (unifying, HIDPP_FEATURE_GETDEVICENAMETYPE);

	if (request[2] == 0) {
		g_debug ("Unable to find GetDeviceNameType feature index");
		return NULL;
	}

	if (write (unifying->priv->fd, request, sizeof(request)) != sizeof(request)) {
		g_debug ("Unable to send GetDeviceNameType.GetCount request to device");
		return NULL;
	}

	start_time = g_get_monotonic_time ();

	while (up_device_unifying_read_response (unifying->priv->fd, buf, sizeof (buf), start_time) > 0)
		if (buf[0] == HIDPP_HEADER_RESPONSE &&
		    buf[1] == unifying->priv->device_index &&
		    buf[2] == request[2] &&
		    buf[3] == HIDPP_FEATURE_GETDEVICENAMETYPE_FUNCTION_GETCOUNT) {
			name_length = buf[4];
			break;
		}

	name = g_string_new_len (NULL, name_length);

	while (name_length > 0) {
		request[3] = HIDPP_FEATURE_GETDEVICENAMETYPE_FUNCTION_GETDEVICENAME;
		request[4] = name->len;

		if (write (unifying->priv->fd, request, sizeof(request)) != sizeof(request)) {
			g_debug ("Unable to send GetDeviceNameType.GetDeviceName request to device");
			g_string_free (name, TRUE);
			return NULL;
		}

		start_time = g_get_monotonic_time ();

		while ((res = up_device_unifying_read_response (unifying->priv->fd, buf,
								sizeof (buf), start_time)) > 0)
			if (buf[0] == HIDPP_HEADER_RESPONSE &&
			    buf[1] == unifying->priv->device_index &&
			    buf[2] == request[2] &&
			    buf[3] == HIDPP_FEATURE_GETDEVICENAMETYPE_FUNCTION_GETDEVICENAME) {
				g_string_append_len (name, (gchar *) &buf[4], MIN(res - 4, name_length));
				name_length -= MIN(res - 4, name_length);
				break;
			}

		/* Handle no response case */
		if (res <= 0) {
			g_debug ("Error reading GetDeviceNameType.GetDeviceName response");
			g_string_free (name, TRUE);
			return NULL;
		}
	}

	return name;
}

/**
 * up_device_unifying_set_device_type:
 *
 * Send a Read Long Register HID++ 1.0 command to the device. This allows to
 * retrieve the type of the device, and then set it.
 **/
static gboolean
up_device_unifying_set_device_type (UpDeviceUnifying *unifying)
{
	guint8 request[] = {
		HIDPP_HEADER_REQUEST,
		HIDPP_RECEIVER_ADDRESS,
		0x83, 0xb5,
		0x20 | (unifying->priv->device_index - 1),
		0x00, 0x00,
	};
	guint8 buf[HIDPP_RESPONSE_LONG_LENGTH];
	UpDevice *device = UP_DEVICE (unifying);
	gint64 start_time;

	if (write (unifying->priv->fd, request, sizeof(request)) != sizeof(request)) {
		g_debug ("Unable to send a HID++ read long register request to device %d",
			 unifying->priv->device_index);
		return FALSE;
	}

	start_time = g_get_monotonic_time ();

	while (up_device_unifying_read_response (unifying->priv->fd, buf, sizeof (buf), start_time) > 0)
		if (buf[0] == HIDPP_HEADER_RESPONSE
		    && buf[1] == HIDPP_RECEIVER_ADDRESS
		    && buf[2] == HIDPP_READ_LONG_REGISTER
		    && buf[3] == 0xb5
		    && buf[4] == (0x20 | (unifying->priv->device_index - 1))) {
			switch (buf[HIDPP_READ_LONG_REGISTER_DEVICE_TYPE]) {
			case HIDPP_READ_LONG_REGISTER_DEVICE_TYPE_KEYBOARD:
			case HIDPP_READ_LONG_REGISTER_DEVICE_TYPE_NUMPAD:
			case HIDPP_READ_LONG_REGISTER_DEVICE_TYPE_REMOTE_CONTROL:
				g_object_set (device, "type", UP_DEVICE_KIND_KEYBOARD, NULL);
				break;

			case HIDPP_READ_LONG_REGISTER_DEVICE_TYPE_MOUSE:
			case HIDPP_READ_LONG_REGISTER_DEVICE_TYPE_TRACKBALL:
			case HIDPP_READ_LONG_REGISTER_DEVICE_TYPE_TOUCHPAD:
			case HIDPP_READ_LONG_REGISTER_DEVICE_TYPE_PRESENTER:
				g_object_set (device, "type", UP_DEVICE_KIND_MOUSE, NULL);
				break;

			case HIDPP_READ_LONG_REGISTER_DEVICE_TYPE_TABLET:
				g_object_set (device, "type", UP_DEVICE_KIND_TABLET, NULL);
				break;

			case HIDPP_READ_LONG_REGISTER_DEVICE_TYPE_GAMEPAD:
			case HIDPP_READ_LONG_REGISTER_DEVICE_TYPE_JOYSTICK:
				/* upower doesn't have something for this yet */
				g_object_set (device, "type", UP_DEVICE_KIND_UNKNOWN, NULL);
				break;
			}
			return TRUE;
		}

	return FALSE;
}

/**
 * up_device_unifying_get_hidpp_version
 *
 * Return the version of HID++ used by a device.
 **/
static gint
up_device_unifying_get_hidpp_version (UpDeviceUnifying *unifying)
{
	guint8 ping[] = {
		HIDPP_HEADER_REQUEST,
		unifying->priv->device_index,
		HIDPP_FEATURE_ROOT_INDEX,
		HIDPP_FEATURE_ROOT_FUNCTION_PING,
		0x00, 0x00, HIDPP_PING_DATA
	};
	guint8 buf[HIDPP_RESPONSE_LONG_LENGTH];
	gint64 start_time;

	if (write(unifying->priv->fd, ping, sizeof(ping)) != sizeof(ping)) {
		g_debug ("Unable to send a HID++ ping to device %d",
			 unifying->priv->device_index);
		return -1;
	}

	/* read event */

	start_time = g_get_monotonic_time ();

	while (up_device_unifying_read_response (unifying->priv->fd, buf, sizeof (buf), start_time) > 0)
		if(buf[0] == HIDPP_HEADER_REQUEST
		   && buf[1] == unifying->priv->device_index
		   && buf[2] == HIDPP_ERR_INVALID_SUBID
		   && buf[3] == 0x00
		   && buf[4] == HIDPP_FEATURE_ROOT_FUNCTION_PING) {
			/* HID++ 1.0 ping reply  */
			if (buf[5] == 0x01)
				return 1;
			else if (buf[5] == 0x09)
				/* device offline / unreachable */
				return 0;
		} else if (buf[0] == HIDPP_HEADER_RESPONSE
			   && buf[1] == unifying->priv->device_index
			   && buf[2] == HIDPP_FEATURE_ROOT_INDEX
			   && buf[3] == HIDPP_FEATURE_ROOT_FUNCTION_PING
			   && buf[6] == HIDPP_PING_DATA)
			/* HID++ >= 2.0 ping reply: buf[4] is major
			   version, buf[5] is minor version but we
			   only care about major for now*/
			return buf[4];

	return -1;
}

/**
 * up_device_unifying_refresh:
 *
 * Return %TRUE on success, %FALSE if we failed to refresh or no data
 **/
static gboolean
up_device_unifying_refresh (UpDevice *device)
{
	UpDeviceUnifying *unifying = UP_DEVICE_UNIFYING (device);
	gint hidpp_version = up_device_unifying_get_hidpp_version (unifying);
	GString *name;
	char *model;
	GTimeVal timeval;

	if (hidpp_version > 0)
		g_debug ("Unifying device %d uses HID++ version %d",
			 unifying->priv->device_index, hidpp_version);

	switch (hidpp_version) {
	case 0:
		g_debug ("Unifying device %d is offline",
			 unifying->priv->device_index);
		g_object_set (device,
			      "is-present", FALSE,
			      "state", UP_DEVICE_STATE_UNKNOWN,
			      NULL);
		break;
	case 1:
		g_object_set (device,
			      "state", UP_DEVICE_STATE_DISCHARGING,
			      "is-present", TRUE,
			      NULL);
		up_device_unifying_hidpp1_set_battery (unifying);
		break;
	case 2:
		g_object_set (device,
			      "is-present", TRUE,
			      NULL);

		g_object_get (device, "model", &model, NULL);
		if (!model) {
			name = up_device_unifying_hidpp2_get_device_name (unifying);
			if (name) {
				g_object_set (device, "model", name->str, NULL);
				g_string_free (name, TRUE);
			}
		} else
			g_free (model);
		up_device_unifying_hidpp2_set_battery (unifying);
		break;
	}

	g_get_current_time (&timeval);
	g_object_set (device, "update-time", (guint64) timeval.tv_sec, NULL);

	return TRUE;
}

/**
 * up_device_unifying_coldplug:
 *
 * Return %TRUE on success, %FALSE if we failed to get data and should be removed
 **/
static gboolean
up_device_unifying_coldplug (UpDevice *device)
{
	UpDeviceUnifying *unifying = UP_DEVICE_UNIFYING (device);
	GUdevDevice *native;
	const gchar *device_file;
	const gchar *vendor;
	const gchar *parent_sysfs_path;
	const gchar *bus_address;
	GList *hidraw_list, *entry;
	size_t len;
	GIOStatus status;
	GError *error = NULL;
	GUdevClient *gudev_client;
	GUdevDevice *parent, *hidraw, *receiver = NULL;
	gboolean ret = FALSE;

	native = G_UDEV_DEVICE (up_device_get_native (device));

	if(g_strcmp0(g_udev_device_get_property (native, "ID_VENDOR_ID"),
		     USB_VENDOR_ID_LOGITECH) ||
	   (g_strcmp0(g_udev_device_get_property (native, "ID_MODEL_ID"),
		      USB_DEVICE_ID_UNIFYING_RECEIVER) &&
	    g_strcmp0(g_udev_device_get_property (native, "ID_MODEL_ID"),
		      USB_DEVICE_ID_UNIFYING_RECEIVER_2))) {
		g_debug ("Not an Unifying device, ignoring");
		return FALSE;
	}

	bus_address = g_udev_device_get_property (native, "PHYS");

	if (!bus_address) {
		g_debug ("Device has no physical bus address, ignoring");
		return FALSE;
	}

	len = strlen (bus_address);

	if (len < 3 || bus_address[len - 3] != ':' || !g_ascii_isdigit (bus_address[len - 2])) {
		g_debug ("Invalid Unifying device index, ignoring");
		return FALSE;
	}

	unifying->priv->device_index = g_ascii_digit_value (bus_address[len - 2]);

	/* Find the hidraw device of the parent (the receiver) to
	 * communicate with the devices */
	gudev_client = g_udev_client_new (NULL);

	parent = g_udev_device_get_parent (native);
	parent_sysfs_path = g_udev_device_get_sysfs_path (parent);
	g_object_unref (parent);

	hidraw_list = g_udev_client_query_by_subsystem (gudev_client, "hidraw");

	for (entry = hidraw_list; entry; entry = entry->next) {
		hidraw = entry->data;
		if (!g_strcmp0 (g_udev_device_get_sysfs_attr (hidraw, "device"),
				parent_sysfs_path))
			receiver = hidraw;
		else
			g_object_unref (hidraw);
	}

	if (!receiver) {
		g_debug ("Unable to find an hidraw device for Unifying receiver");
		return FALSE;
	}

	/* get device file */
	device_file = g_udev_device_get_device_file (receiver);

	/* connect to the device */
	g_debug ("Using Unifying receiver hidraw device file: %s", device_file);

	if (device_file == NULL) {
		g_debug ("Could not get device file for Unifying receiver device");
		goto out;
	}

	unifying->priv->fd = open (device_file, O_RDWR | O_NONBLOCK);
	if (unifying->priv->fd < 0) {
		g_debug ("cannot open device file %s", device_file);
		return FALSE;
	}

	vendor = g_udev_device_get_property (native, "ID_VENDOR");

	/* hardcode some default values */
	g_object_set (device,
		      "vendor", vendor,
		      "is-present", TRUE,
		      "has-history", TRUE,
		      "is-rechargeable", TRUE,
		      "state", UP_DEVICE_STATE_DISCHARGING,
		      "power-supply", FALSE,
		      NULL);

	/* Set device type */
	if (!up_device_unifying_set_device_type(unifying)) {
		g_debug ("Unable to guess device type, ignoring the device");
		goto out;
	}

	unifying->priv->channel = g_io_channel_unix_new (unifying->priv->fd);

	/* set binary encoding */
	status = g_io_channel_set_encoding (unifying->priv->channel, NULL, &error);
	if (status != G_IO_STATUS_NORMAL) {
		g_warning ("failed to set encoding: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* watch this */
	unifying->priv->channel_source_id = g_io_add_watch (unifying->priv->channel,
							    G_IO_IN,
							    up_device_unifying_event_io,
							    unifying);

	/* set up a poll to send the magic packet */
	unifying->priv->poll_timer_id = g_timeout_add_seconds (UP_DEVICE_UNIFYING_REFRESH_TIMEOUT,
							       (GSourceFunc) up_device_unifying_refresh,
							       device);

	ret = TRUE;

 out:
	g_object_unref (gudev_client);
	g_object_unref (receiver);
	g_list_free (hidraw_list);

	if (!ret && unifying->priv->fd >= 0)
		close (unifying->priv->fd);

	return ret;
}

/**
 * up_device_unifying_init:
 **/
static void
up_device_unifying_init (UpDeviceUnifying *unifying)
{
	unifying->priv = UP_DEVICE_UNIFYING_GET_PRIVATE (unifying);
	unifying->priv->poll_timer_id = 0;
	unifying->priv->fd = -1;
	unifying->priv->feature_k750_battery_index = -1;
}

/**
 * up_device_unifying_finalize:
 **/
static void
up_device_unifying_finalize (GObject *object)
{
	UpDeviceUnifying *unifying;

	g_return_if_fail (object != NULL);
	g_return_if_fail (UP_IS_DEVICE_UNIFYING (object));

	unifying = UP_DEVICE_UNIFYING (object);
	g_return_if_fail (unifying->priv != NULL);

	if (unifying->priv->poll_timer_id > 0)
		g_source_remove (unifying->priv->poll_timer_id);

	if (unifying->priv->channel_source_id > 0)
		g_source_remove (unifying->priv->channel_source_id);

	if (unifying->priv->channel) {
		g_io_channel_shutdown (unifying->priv->channel, FALSE, NULL);
		g_io_channel_unref (unifying->priv->channel);
	}

	G_OBJECT_CLASS (up_device_unifying_parent_class)->finalize (object);
}

/**
 * up_device_unifying_class_init:
 **/
static void
up_device_unifying_class_init (UpDeviceUnifyingClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	UpDeviceClass *device_class = UP_DEVICE_CLASS (klass);

	object_class->finalize = up_device_unifying_finalize;
	device_class->coldplug = up_device_unifying_coldplug;
	device_class->refresh = up_device_unifying_refresh;

	g_type_class_add_private (klass, sizeof (UpDeviceUnifyingPrivate));
}

/**
 * up_device_unifying_new:
 **/
UpDeviceUnifying *
up_device_unifying_new (void)
{
	return g_object_new (UP_TYPE_DEVICE_UNIFYING, NULL);
}

