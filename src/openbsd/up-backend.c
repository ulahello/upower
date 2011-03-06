#include "up-apm-native.h"

#include "up-backend.h"
#include "up-daemon.h"
#include "up-marshal.h"
#include "up-device.h"

#define UP_BACKEND_SUSPEND_COMMAND	"/usr/sbin/zzz"

static void	up_backend_class_init	(UpBackendClass	*klass);
static void	up_backend_init	(UpBackend		*backend);
static void	up_backend_finalize	(GObject		*object);

static void	up_backend_apm_get_power_info(int, struct apm_power_info*);
UpDeviceState up_backend_apm_get_battery_state_value(u_char battery_state);

static gboolean		up_apm_device_get_on_battery	(UpDevice *device, gboolean *on_battery);
static gboolean		up_apm_device_get_low_battery	(UpDevice *device, gboolean *low_battery);
static gboolean		up_apm_device_get_online		(UpDevice *device, gboolean *online);

#define UP_BACKEND_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), UP_TYPE_BACKEND, UpBackendPrivate))

struct UpBackendPrivate
{
	UpDaemon		*daemon;
	UpDevice		*ac;
	UpDevice		*battery;
	GThread			*apm_thread;
	gboolean		is_laptop;
	int			apm_fd;
};

enum {
	SIGNAL_DEVICE_ADDED,
	SIGNAL_DEVICE_REMOVED,
	SIGNAL_LAST
};

static guint signals [SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE (UpBackend, up_backend, G_TYPE_OBJECT)

/**
 * functions called by upower daemon
 **/


/* those three ripped from freebsd/up-device-supply.c */
gboolean
up_apm_device_get_on_battery (UpDevice *device, gboolean * on_battery)
{
	UpDeviceKind type;
	UpDeviceState state;
	gboolean is_present;

	g_return_val_if_fail (on_battery != NULL, FALSE);

	g_object_get (device,
		      "type", &type,
		      "state", &state,
		      "is-present", &is_present,
		      (void*) NULL);

	if (type != UP_DEVICE_KIND_BATTERY)
		return FALSE;
	if (state == UP_DEVICE_STATE_UNKNOWN)
		return FALSE;
	if (!is_present)
		return FALSE;

	*on_battery = (state == UP_DEVICE_STATE_DISCHARGING);
	return TRUE;
}
gboolean
up_apm_device_get_low_battery (UpDevice *device, gboolean * low_battery)
{
	gboolean ret;
	gboolean on_battery;
	gdouble percentage;

	g_return_val_if_fail (low_battery != NULL, FALSE);

	ret = up_apm_device_get_on_battery (device, &on_battery);
	if (!ret)
		return FALSE;

	if (!on_battery) {
		*low_battery = FALSE;
		return TRUE;
	}

	g_object_get (device, "percentage", &percentage, (void*) NULL);
	*low_battery = (percentage < 10.0f);
	return TRUE;
}

gboolean
up_apm_device_get_online (UpDevice *device, gboolean * online)
{
	UpDeviceKind type;
	gboolean online_tmp;

	g_return_val_if_fail (online != NULL, FALSE);

	g_object_get (device,
		      "type", &type,
		      "online", &online_tmp,
		      (void*) NULL);

	if (type != UP_DEVICE_KIND_LINE_POWER)
		return FALSE;

	*online = online_tmp;

	return TRUE;
}
/**
 * up_backend_coldplug:
 * @backend: The %UpBackend class instance
 * @daemon: The %UpDaemon controlling instance
 *
 * Finds all the devices already plugged in, and emits device-add signals for
 * each of them.
 *
 * Return value: %TRUE for success
 **/
gboolean
up_backend_coldplug (UpBackend *backend, UpDaemon *daemon)
{
	UpApmNative *acnative = NULL;
	UpApmNative *battnative = NULL;
	backend->priv->daemon = g_object_ref (daemon);
	/* small delay until first device is added */
	if (backend->priv->is_laptop)
	{
		acnative = up_apm_native_new("/ac");
		if (!up_device_coldplug (backend->priv->ac, backend->priv->daemon, G_OBJECT(acnative)))
			g_warning ("failed to coldplug ac");
		else
			g_signal_emit (backend, signals[SIGNAL_DEVICE_ADDED], 0, acnative, backend->priv->ac);

		battnative = up_apm_native_new("/batt");
		if (!up_device_coldplug (backend->priv->battery, backend->priv->daemon, G_OBJECT(battnative)))
			g_warning ("failed to coldplug battery");
		else
			g_signal_emit (backend, signals[SIGNAL_DEVICE_ADDED], 0, battnative, backend->priv->battery);
	}

	return TRUE;
}


/**
 * up_backend_get_powersave_command:
 **/
const gchar *
up_backend_get_powersave_command (UpBackend *backend, gboolean powersave)
{
	return NULL;
}

/**
 * up_backend_get_suspend_command:
 **/
const gchar *
up_backend_get_suspend_command (UpBackend *backend)
{
	return UP_BACKEND_SUSPEND_COMMAND;
}

/**
 * up_backend_get_hibernate_command:
 **/
const gchar *
up_backend_get_hibernate_command (UpBackend *backend)
{
	return NULL;
}

/**
 * up_backend_kernel_can_suspend:
 **/
gboolean
up_backend_kernel_can_suspend (UpBackend *backend)
{
	return TRUE;
}

/**
 * up_backend_kernel_can_hibernate:
 **/
gboolean
up_backend_kernel_can_hibernate (UpBackend *backend)
{
	return FALSE;
}

gboolean
up_backend_has_encrypted_swap (UpBackend *backend)
{
	return FALSE;
}

/* Return value: a percentage value */
gfloat
up_backend_get_used_swap (UpBackend *backend)
{
	return 0;
}

/**
 * OpenBSD specific code
 **/

static void
up_backend_apm_get_power_info(int fd, struct apm_power_info *bstate) {
	bstate->battery_state = 255;
	bstate->ac_state = 255;
	bstate->battery_life = 0;
	bstate->minutes_left = -1;

	if (-1 == ioctl(fd, APM_IOC_GETPOWER, bstate))
		g_warning("ioctl on fd %d failed : %s", fd, g_strerror(errno));
}

UpDeviceState up_backend_apm_get_battery_state_value(u_char battery_state) {
	switch(battery_state) {
		case APM_BATT_HIGH:
			return UP_DEVICE_STATE_FULLY_CHARGED;
		case APM_BATT_LOW:
			return UP_DEVICE_STATE_DISCHARGING; // XXXX
		case APM_BATT_CRITICAL:
			return UP_DEVICE_STATE_EMPTY;
		case APM_BATT_CHARGING:
			return UP_DEVICE_STATE_CHARGING;
		case APM_BATTERY_ABSENT:
			return UP_DEVICE_STATE_EMPTY;
		case APM_BATT_UNKNOWN:
			return UP_DEVICE_STATE_UNKNOWN;
	}
	return -1;
}

static void
up_backend_update_ac_state(UpDevice* device, struct apm_power_info a)
{
	GTimeVal timeval;
	gboolean new_is_online, cur_is_online;
	g_object_get (device, "online", &cur_is_online, (void*) NULL);
	new_is_online = (a.ac_state == APM_AC_ON ? TRUE : FALSE);
	if (cur_is_online != new_is_online)
	{
		g_get_current_time (&timeval);
		g_object_set (device,
			"online", new_is_online,
			"update-time", (guint64) timeval.tv_sec,
			(void*) NULL);
	}
}

static void
up_backend_update_battery_state(UpDevice* device, struct apm_power_info a)
{
	GTimeVal timeval;
	gdouble percentage;
	UpDeviceState cur_state, new_state;
	gint64 cur_time_to_empty, new_time_to_empty;
	g_object_get (device,
		"state", &cur_state,
		"percentage", &percentage,
		"time-to-empty", &cur_time_to_empty,
		(void*) NULL);

	new_state = up_backend_apm_get_battery_state_value(a.battery_state);
	// if percentage/minutes goes down or ac is off, we're likely discharging..
	if (percentage < a.battery_life || cur_time_to_empty < new_time_to_empty || a.ac_state == APM_AC_OFF)
		new_state = UP_DEVICE_STATE_DISCHARGING;

	// zero out new_time_to empty if we're not discharging
	new_time_to_empty = (new_state == UP_DEVICE_STATE_DISCHARGING ? a.minutes_left : 0);

	if (cur_state != new_state ||
		percentage != (gdouble) a.battery_life ||
		cur_time_to_empty != new_time_to_empty)
	{
		g_get_current_time (&timeval);
		g_object_set (device,
			"state", new_state,
			"percentage", (gdouble) a.battery_life,
			"time-to-empty", new_time_to_empty * 60,
			"update-time", (guint64) timeval.tv_sec,
			(void*) NULL);
	}
}

/* callback updating the device */
static gboolean
up_backend_apm_powerchange_event_cb(gpointer object)
{
	UpBackend *backend;
	struct apm_power_info a;

	g_return_if_fail (UP_IS_BACKEND (object));
	backend = UP_BACKEND (object);
	up_backend_apm_get_power_info(backend->priv->apm_fd, &a);

	g_debug("Got apm event, in callback, percentage=%d, battstate=%d, acstate=%d, minutes left=%d", a.battery_life, a.battery_state, a.ac_state, a.minutes_left);
	up_backend_update_ac_state(backend->priv->ac, a);
	up_backend_update_battery_state(backend->priv->battery, a);
	/* return false to not endless loop */
	return FALSE;
}

/* thread doing kqueue() on apm device */
static gpointer
up_backend_apm_event_thread(gpointer object)
{
	int kq, nevents;
	struct kevent ev;
	struct timespec ts = {600, 0}, sts = {0, 0};

	UpBackend *backend;

	g_return_if_fail (UP_IS_BACKEND (object));
	backend = UP_BACKEND (object);

	g_debug("setting up apm thread");

	/* open /dev/apm */
	if ((backend->priv->apm_fd = open("/dev/apm", O_RDONLY)) == -1) {
		if (errno != ENXIO && errno != ENOENT)
			g_error("cannot open device file");
	}
	kq = kqueue();
	if (kq <= 0)
		g_error("kqueue");
	EV_SET(&ev, backend->priv->apm_fd, EVFILT_READ, EV_ADD | EV_ENABLE | EV_CLEAR,
	    0, 0, NULL);
	nevents = 1;
	if (kevent(kq, &ev, nevents, NULL, 0, &sts) < 0)
		g_error("kevent");

	/* blocking wait on kqueue */
	for (;;) {
		int rv;

		/* 10mn timeout */
		sts = ts;
		if ((rv = kevent(kq, NULL, 0, &ev, 1, &sts)) < 0)
			break;
		if (!rv)
			continue;
		if (ev.ident == (guint) backend->priv->apm_fd && APM_EVENT_TYPE(ev.data) == APM_POWER_CHANGE ) {
			/* g_idle_add the callback */
			g_idle_add((GSourceFunc) up_backend_apm_powerchange_event_cb, backend);
		}
	}
	return NULL;
	/* shouldnt be reached ? */
}

/**
 * GObject class functions
 **/

/**
 * up_backend_new:
 *
 * Return value: a new %UpBackend object.
 **/
UpBackend *
up_backend_new (void)
{
	UpBackend *backend;
	backend = g_object_new (UP_TYPE_BACKEND, NULL);
	return UP_BACKEND (backend);
}

/**
 * up_backend_class_init:
 * @klass: The UpBackendClass
 **/
static void
up_backend_class_init (UpBackendClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = up_backend_finalize;

	signals [SIGNAL_DEVICE_ADDED] =
		g_signal_new ("device-added",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (UpBackendClass, device_added),
			      NULL, NULL, up_marshal_VOID__POINTER_POINTER,
			      G_TYPE_NONE, 2, G_TYPE_POINTER, G_TYPE_POINTER);
	signals [SIGNAL_DEVICE_REMOVED] =
		g_signal_new ("device-removed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (UpBackendClass, device_removed),
			      NULL, NULL, up_marshal_VOID__POINTER_POINTER,
			      G_TYPE_NONE, 2, G_TYPE_POINTER, G_TYPE_POINTER);

	g_type_class_add_private (klass, sizeof (UpBackendPrivate));
}

/**
 * up_backend_init:
 **/
static void
up_backend_init (UpBackend *backend)
{
	GError *err = NULL;
	UpDeviceClass *device_class;

	backend->priv = UP_BACKEND_GET_PRIVATE (backend);
	backend->priv->daemon = NULL;
	backend->priv->is_laptop = up_native_is_laptop();
	g_debug("is_laptop:%d",backend->priv->is_laptop);
	if (backend->priv->is_laptop)
	{
		backend->priv->ac = UP_DEVICE(up_device_new());
		backend->priv->battery = UP_DEVICE(up_device_new ());
		device_class = UP_DEVICE_GET_CLASS (backend->priv->battery);
		device_class->get_on_battery = up_apm_device_get_on_battery;
		device_class->get_low_battery = up_apm_device_get_low_battery;
		device_class->get_online = up_apm_device_get_online;
		device_class = UP_DEVICE_GET_CLASS (backend->priv->ac);
		device_class->get_on_battery = up_apm_device_get_on_battery;
		device_class->get_low_battery = up_apm_device_get_low_battery;
		device_class->get_online = up_apm_device_get_online;
		g_thread_init (NULL);
		/* creates thread */
		if((backend->priv->apm_thread = (GThread*) g_thread_create((GThreadFunc)up_backend_apm_event_thread, backend, FALSE, &err) == NULL))
		{
			g_warning("Thread create failed: %s", err->message);
			g_error_free (err);
		}

		/* setup dummy */
		g_object_set (backend->priv->battery,
			      "type", UP_DEVICE_KIND_BATTERY,
			      "power-supply", TRUE,
			      "is-present", TRUE,
			      "is-rechargeable", TRUE,
			      "has-history", TRUE,
			      "has-statistics", TRUE,
			      "state", UP_DEVICE_STATE_UNKNOWN,
			      "percentage", 0.0f,
			      "time-to-empty", 0,
			      (void*) NULL);
		g_object_set (backend->priv->ac,
			      "type", UP_DEVICE_KIND_LINE_POWER,
			      "online", TRUE,
			      "power-supply", TRUE,
			      (void*) NULL);
	} else {
		backend->priv->ac = NULL;
		backend->priv->battery = NULL;
	}
}
/**
 * up_backend_finalize:
 **/
static void
up_backend_finalize (GObject *object)
{
	UpBackend *backend;

	g_return_if_fail (UP_IS_BACKEND (object));

	backend = UP_BACKEND (object);

	if (backend->priv->daemon != NULL)
		g_object_unref (backend->priv->daemon);
	if (backend->priv->battery != NULL)
		g_object_unref (backend->priv->battery);
	if (backend->priv->ac != NULL)
		g_object_unref (backend->priv->ac);
	/* XXX stop apm_thread ? */

	G_OBJECT_CLASS (up_backend_parent_class)->finalize (object);
}

