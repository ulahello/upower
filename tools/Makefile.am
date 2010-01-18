## Process this file with automake to produce Makefile.in

INCLUDES = \
	-DPACKAGE_LIBEXEC_DIR=\""$(libexecdir)"\"		\
	-DPACKAGE_SYSCONF_DIR=\""$(sysconfdir)"\"		\
	-DPACKAGE_DATA_DIR=\""$(datadir)"\"			\
	-DPACKAGE_BIN_DIR=\""$(bindir)"\"			\
	-DPACKAGE_LOCALSTATE_DIR=\""$(localstatedir)"\"		\
	-DPACKAGE_LOCALE_DIR=\""$(localedir)"\"			\
	-DPACKAGE_LIB_DIR=\""$(libdir)"\"			\
	-D_POSIX_PTHREAD_SEMANTICS -D_REENTRANT			\
	-DUP_COMPILATION					\
	-DUP_DISABLE_DEPRECATED				\
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

BUILT_SOURCES =						\
	up-daemon-glue.h				\
	up-marshal.h		up-marshal.c

up-marshal.h: $(top_srcdir)/src/up-marshal.list
	glib-genmarshal $< --prefix=devkit_power_marshal --header > $@

up-marshal.c: $(top_srcdir)/src/up-marshal.list
	echo "#include \"up-marshal.h\"" > $@ && glib-genmarshal $< --prefix=devkit_power_marshal --body >> $@

up-daemon-glue.h: $(top_srcdir)/src/org.freedesktop.UPower.xml Makefile.am
	dbus-binding-tool --prefix=devkit_power_daemon --mode=glib-client --output=up-daemon-glue.h $(top_srcdir)/src/org.freedesktop.UPower.xml

bin_PROGRAMS = devkit-power

devkit_power_SOURCES = 					\
	egg-debug.c					\
	egg-debug.h					\
	up-tool.c					\
	$(BUILT_SOURCES)

devkit_power_CPPFLAGS = 				\
	-DG_LOG_DOMAIN=\"devkit-power\"		        \
	$(DISABLE_DEPRECATED)				\
	$(AM_CPPFLAGS)

devkit_power_LDADD = 					\
	$(DBUS_GLIB_LIBS)				\
	$(DEVKIT_POWER_LIBS)				\
	$(POLKIT_DBUS_LIBS)

CLEANFILES = $(BUILT_SOURCES)

clean-local :
	rm -f *~ $(service_DATA) $(dbusconf_DATA)
