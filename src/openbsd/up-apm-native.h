#ifndef __UP_APM_NATIVE_H__
#define __UP_APM_NATIVE_H__

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

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


// XX in .c ?
//GType up_apm_native_get_type (void);
//G_DEFINE_TYPE (UpApmNative, up_apm_native, G_TYPE_OBJECT)

UpApmNative* up_apm_native_new (const char*);
const gchar * up_apm_native_get_path(UpApmNative*);

G_END_DECLS

#endif
