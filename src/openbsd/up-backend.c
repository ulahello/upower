
#include "up-backend.h"
#include "up-daemon.h"

#define UP_BACKEND_SUSPEND_COMMAND	"/usr/sbin/zzz"

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
	return FALSE;
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
 * up_backend_new:
 *
 * Return value: a new %UpBackend object.
 **/
UpBackend *
up_backend_new (void)
{
	return NULL;
}
