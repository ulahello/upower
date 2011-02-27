#include "up-native.h"

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
	return "/foo"; /* return sysctl acpiacX/acpibatX path ? */
}
