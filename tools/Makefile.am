## Process this file with automake to produce Makefile.in

INCLUDES = \
	-DUP_COMPILATION					\
	-DEGG_LOG_FILE=\""$(UP_LOG_DIR)/DeviceKit-power"\"	\
	-DEGG_VERBOSE="\"DKP_VERBOSE\""				\
	-DEGG_LOGGING="\"DKP_LOGGING\""				\
	-DEGG_CONSOLE="\"DKP_CONSOLE\""				\
	-I$(top_srcdir)						\
	-I$(top_srcdir)/devkit-power-gobject			\
	$(DBUS_GLIB_CFLAGS)					\
	$(POLKIT_DBUS_CFLAGS)					\
	$(GLIB_CFLAGS)

DEVKIT_POWER_LIBS = $(top_builddir)/devkit-power-gobject/libdevkit-power-gobject.la

bin_PROGRAMS = upower

upower_SOURCES = 					\
	egg-debug.c					\
	egg-debug.h					\
	up-tool.c					\
	$(BUILT_SOURCES)

upower_CPPFLAGS = 					\
	$(AM_CPPFLAGS)

upower_LDADD = 						\
	$(DBUS_GLIB_LIBS)				\
	$(DEVKIT_POWER_LIBS)				\
	$(POLKIT_DBUS_LIBS)

install-exec-hook:
	cd $(DESTDIR)$(bindir) && $(LN_S) upower devkit-power

CLEANFILES = $(BUILT_SOURCES)

clean-local :
	rm -f *~ $(service_DATA) $(dbusconf_DATA)
