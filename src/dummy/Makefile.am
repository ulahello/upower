## Process this file with automake to produce Makefile.in

INCLUDES = \
	-I$(top_builddir)/src -I$(top_srcdir)/src		\
	-DEGG_LOG_FILE=\""$(UP_LOG_DIR)/DeviceKit-power"\"	\
	-DEGG_VERBOSE="\"UP_VERBOSE\""				\
	-DEGG_LOGGING="\"UP_LOGGING\""				\
	-DEGG_CONSOLE="\"UP_CONSOLE\""				\
	-DUP_COMPILATION					\
	-DEGG_TEST						\
	-I$(top_srcdir)/devkit-power-gobject			\
	$(DBUS_GLIB_CFLAGS)					\
	$(POLKIT_CFLAGS)					\
	$(GLIB_CFLAGS)

noinst_LTLIBRARIES = libupshared.la

libupshared_la_SOURCES =					\
	up-backend.c						\
	up-native.c						\
	$(BUILT_SOURCES)

clean-local :
	rm -f *~

