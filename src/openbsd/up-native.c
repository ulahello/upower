#include "up-apm-native.h"
#include "up-native.h"

#include <sys/param.h>
#include <sys/sensors.h>
#include <sys/sysctl.h>
#include <errno.h>
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

	if (up_native_has_sensor("acpiac0"))
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
 * detect if a sensordev is present by its xname (acpibatX/acpiacX)
 */
gboolean
up_native_has_sensor(const char * id)
{
	int devn;
	struct sensordev snsrdev;
	size_t sdlen = sizeof(snsrdev);
	int mib[] = {CTL_HW, HW_SENSORS, 0, 0 ,0};

	for (devn = 0 ; ; devn++) {
		mib[2] = devn;
		if (sysctl(mib, 3, &snsrdev, &sdlen, NULL, 0) == -1) {
			if (errno == ENXIO)
				continue;
			if (errno == ENOENT)
				break;
		}
		if (!strcmp(snsrdev.xname, id))
			return TRUE;
	}
	return FALSE;
}
