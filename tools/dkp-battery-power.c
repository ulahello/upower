#include <glib.h>
#include <dbus/dbus-glib.h>
#include "dkp-debug.h"

/**
 * main:
 **/
int
main (int argc, char **argv)
{
	gint retval = 1;
	gboolean ret;
	gboolean on_battery;
	GError *error = NULL;
	DBusGConnection *bus = NULL;
	DBusGProxy *proxy = NULL;
	gboolean verbose = FALSE;
	GOptionContext *context;

	const GOptionEntry entries[] = {
		{ "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose, "Show extra debugging information", NULL },
		{ NULL }
	};

	g_type_init ();

	context = g_option_context_new ("devkit-battery-power");
	g_option_context_add_main_entries (context, entries, NULL);
	g_option_context_parse (context, &argc, &argv, NULL);
	g_option_context_free (context);
	dkp_debug_init (verbose);

	bus = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
	if (bus == NULL) {
		dkp_warning ("Couldn't connect to system bus: %s", error->message);
		g_error_free (error);
		goto out;
	}

	proxy = dbus_g_proxy_new_for_name (bus, "org.freedesktop.DeviceKit.Power",
					   "/", "org.freedesktop.DeviceKit.Power");
	if (proxy == NULL) {
		dkp_warning ("Couldn't connect to DeviceKit-power");
		goto out;
	}

	ret = dbus_g_proxy_call (proxy, "GetOnBattery", &error,
				 G_TYPE_INVALID,
				 G_TYPE_BOOLEAN, &on_battery,
				 G_TYPE_INVALID);
	if (!ret) {
		dkp_debug ("GetOnBattery failed: %s", error->message);
		g_error_free (error);
		goto out;
	}

	g_print ("on-battery: %s\n", on_battery ? "yes" : "no");
	retval = 0;

out:
	if (proxy != NULL)
		g_object_unref (proxy);
	if (bus != NULL)
		dbus_g_connection_unref (bus);
	return retval;
}

