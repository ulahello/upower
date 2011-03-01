#include "up-native.h"

#define UP_TYPE_APM_NATIVE		(up_apm_native_get_type ())
#define UP_APM_NATIVE(o)	   	(G_TYPE_CHECK_INSTANCE_CAST ((o), UP_TYPE_APM_NATIVE, UpApmNative))
#define UP_APM_NATIVE_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), UP_TYPE_APM_NATIVE, UpApmNativeClass))
#define UP_IS_APM_NATIVE(o)	(G_TYPE_CHECK_INSTANCE_TYPE ((o), UP_TYPE_APM_NATIVE))
#define UP_IS_APM_NATIVE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), UP_TYPE_APM_NATIVE))
#define UP_APM_NATIVE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), UP_TYPE_APM_NATIVE, UpApmNativeClass))

typedef struct
{
	GObject	parent;
	gchar*	path;
} UpApmNative;

typedef struct
{
	GObjectClass	parent_class;
} UpApmNativeClass;

G_DEFINE_TYPE (UpApmNative, up_apm_native, G_TYPE_OBJECT)

GType up_apm_native_get_type(void);

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
