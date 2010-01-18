## Process this file with automake to produce Makefile.in

INCLUDES = \
	-I$(top_builddir)/src -I$(top_srcdir)/src		\
	-DEGG_LOG_FILE=\""$(DKP_LOG_DIR)/DeviceKit-power"\"	\
	-DEGG_VERBOSE="\"DKP_VERBOSE\""				\
	-DEGG_LOGGING="\"DKP_LOGGING\""				\
	-DEGG_CONSOLE="\"DKP_CONSOLE\""				\
	-DDKP_COMPILATION					\
	-DG_UDEV_API_IS_SUBJECT_TO_CHANGE			\
	-I$(top_srcdir)/devkit-power-gobject			\
	$(GIO_CFLAGS)						\
	$(DBUS_GLIB_CFLAGS)					\
	$(GUDEV_CFLAGS)						\
	$(POLKIT_CFLAGS)					\
	$(GLIB_CFLAGS)

if BACKEND_TYPE_LINUX
noinst_LTLIBRARIES = libdkpshared.la
endif

libdkpshared_la_SOURCES =					\
	up-device-supply.c					\
	up-device-supply.h					\
	up-device-csr.c						\
	up-device-csr.h						\
	up-device-hid.c						\
	up-device-hid.h						\
	up-device-wup.c						\
	up-device-wup.h						\
	up-input.c						\
	up-input.h						\
	up-backend.c						\
	up-native.c						\
	sysfs-utils.c						\
	sysfs-utils.h						\
	$(BUILT_SOURCES)

libdkpshared_la_CFLAGS =					\
	$(WARNINGFLAGS_C)

clean-local :
	rm -f *~

