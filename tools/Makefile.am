## Process this file with automake to produce Makefile.in

INCLUDES = \
	-I$(top_builddir)/src -I$(top_srcdir)/src \
	-DPACKAGE_LIBEXEC_DIR=\""$(libexecdir)"\" \
	-DPACKAGE_SYSCONF_DIR=\""$(sysconfdir)"\" \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_BIN_DIR=\""$(bindir)"\" \
	-DPACKAGE_LOCALSTATE_DIR=\""$(localstatedir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(localedir)"\" \
	-DPACKAGE_LIB_DIR=\""$(libdir)"\" \
	-D_POSIX_PTHREAD_SEMANTICS -D_REENTRANT	\
	-I$(top_srcdir)/libdevkit-power \
	$(DBUS_GLIB_CFLAGS) \
	$(POLKIT_DBUS_CFLAGS) \
	$(GLIB_CFLAGS)

DEVKIT_POWER_LIBS = $(top_builddir)/libdevkit-power/libdevkit-power.la

BUILT_SOURCES =						\
	dkp-daemon-glue.h				\
	dkp-marshal.h		dkp-marshal.c

dkp-marshal.h: $(top_srcdir)/src/dkp-marshal.list
	glib-genmarshal $< --prefix=devkit_power_marshal --header > $@

dkp-marshal.c: $(top_srcdir)/src/dkp-marshal.list
	echo "#include \"dkp-marshal.h\"" > $@ && glib-genmarshal $< --prefix=devkit_power_marshal --body >> $@

dkp-daemon-glue.h: $(top_srcdir)/src/org.freedesktop.DeviceKit.Power.xml Makefile.am
	dbus-binding-tool --prefix=devkit_power_daemon --mode=glib-client --output=dkp-daemon-glue.h $(top_srcdir)/src/org.freedesktop.DeviceKit.Power.xml

bin_PROGRAMS = devkit-power devkit-power-on-battery

devkit_power_SOURCES = 					\
	dkp-tool.c					\
	$(BUILT_SOURCES)

devkit_power_CPPFLAGS = 				\
	-DG_LOG_DOMAIN=\"devkit-power\"		        \
	$(DISABLE_DEPRECATED)				\
	$(AM_CPPFLAGS)

devkit_power_LDADD = 					\
	$(DBUS_GLIB_LIBS)				\
	$(DEVKIT_POWER_LIBS)				\
	$(POLKIT_DBUS_LIBS)

devkit_power_on_battery_SOURCES = 			\
	dkp-battery-power.c				\
	$(BUILT_SOURCES)

devkit_power_on_battery_CPPFLAGS = 			\
	-DG_LOG_DOMAIN=\"devkit-power\"		        \
	$(DISABLE_DEPRECATED)				\
	$(AM_CPPFLAGS)

devkit_power_on_battery_LDADD = 			\
	$(DBUS_GLIB_LIBS)				\
	$(DEVKIT_POWER_LIBS)				\
	$(POLKIT_DBUS_LIBS)

CLEANFILES = $(BUILT_SOURCES)

clean-local :
	rm -f *~ $(service_DATA) $(dbusconf_DATA)
