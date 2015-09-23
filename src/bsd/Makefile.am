## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = \
	-I$(top_builddir)/src -I$(top_srcdir)/src		\
	-I$(top_builddir)/ -I$(top_srcdir)/			\
	-DUP_COMPILATION					\
	-DG_LOG_DOMAIN=\"UPower-Unix\"			\
	-I$(top_srcdir)/libupower-glib				\
	$(POLKIT_CFLAGS)					\
	$(DBUS_GLIB_CFLAGS)					\
	$(GLIB_CFLAGS)						\
	$(GIO_CFLAGS)

noinst_LTLIBRARIES = libupsharedcommon.la

libupsharedcommon_la_SOURCES =					\
	up-backend-common.c							\
	$(BUILT_SOURCES)

libupsharedcommon_la_CFLAGS =					\
	$(WARNINGFLAGS_C)

EXTRA_DIST = $(libupsharedcommon_la_SOURCES)

libupsharedcommon_la_LIBADD = 					\
	$(GIO_LIBS)									\
	$(GLIB_LIBS)

clean-local :
	rm -f *~

-include $(top_srcdir)/git.mk
