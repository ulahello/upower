## Process this file with automake to produce Makefile.in

INCLUDES = \
	-I$(top_builddir)/src -I$(top_srcdir)/src		\
	-DUP_COMPILATION					\
	-DEGG_TEST						\
	-I$(top_srcdir)/devkit-power-gobject			\
	-I$(top_srcdir)/libupower-glib				\
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

