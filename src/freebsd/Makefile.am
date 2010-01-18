## Process this file with automake to produce Makefile.in

INCLUDES = \
	-I$(top_builddir)/src -I$(top_srcdir)/src		\
	-DEGG_LOG_FILE=\""$(DKP_LOG_DIR)/DeviceKit-power"\"	\
	-DEGG_VERBOSE="\"DKP_VERBOSE\""				\
	-DEGG_LOGGING="\"DKP_LOGGING\""				\
	-DEGG_CONSOLE="\"DKP_CONSOLE\""				\
	-DDKP_COMPILATION					\
	-I$(top_srcdir)/devkit-power-gobject			\
	$(DBUS_GLIB_CFLAGS)					\
	$(POLKIT_CFLAGS)					\
	$(GLIB_CFLAGS)

if BACKEND_TYPE_FREEBSD
noinst_LTLIBRARIES = libdkpshared.la
endif

libdkpshared_la_SOURCES =					\
	up-acpi-native.c					\
	up-acpi-native.h					\
	up-backend-acpi.h					\
	up-backend.c						\
	up-devd.c						\
	up-devd.h						\
	up-device-supply.c					\
	up-device-supply.h					\
	up-native.c						\
	up-util.c						\
	up-util.h						\
	$(BUILT_SOURCES)

libdkpshared_la_CFLAGS =					\
	$(WARNINGFLAGS_C)

libdkpshared_la_LIBADD =					\
	-lkvm

EXTRA_DIST =							\
	up-acpi-native.vala					\
	TODO

clean-local :
	rm -f *~

