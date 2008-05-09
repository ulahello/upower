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
	$(DBUS_GLIB_CFLAGS) \
	$(POLKIT_DBUS_CFLAGS) \
	$(DEVKIT_CFLAGS) \
	$(GLIB_CFLAGS)

BUILT_SOURCES =								\
	devkit-power-daemon-glue.h					\
	devkit-power-source-glue.h					\
	devkit-power-marshal.h			devkit-power-marshal.c

devkit-power-marshal.h: devkit-power-marshal.list
	glib-genmarshal $< --prefix=devkit_power_marshal --header > $@

devkit-power-marshal.c: devkit-power-marshal.list
	echo "#include \"devkit-power-marshal.h\"" > $@ && glib-genmarshal $< --prefix=devkit_power_marshal --body >> $@

devkit-power-daemon-glue.h: org.freedesktop.DeviceKit.Power.xml Makefile.am
	dbus-binding-tool --prefix=devkit_power_daemon --mode=glib-server --output=devkit-power-daemon-glue.h org.freedesktop.DeviceKit.Power.xml

devkit-power-source-glue.h: org.freedesktop.DeviceKit.Power.Source.xml Makefile.am
	dbus-binding-tool --prefix=devkit_power_source --mode=glib-server --output=devkit-power-source-glue.h org.freedesktop.DeviceKit.Power.Source.xml

libexec_PROGRAMS = devkit-power-daemon

dbusifdir = $(datadir)/dbus-1/interfaces
dbusif_DATA = org.freedesktop.DeviceKit.Power.xml org.freedesktop.DeviceKit.Power.Source.xml

devkit_power_daemon_SOURCES = 							\
	devkit-power-daemon.h			devkit-power-daemon.c		\
	devkit-power-device.h			devkit-power-device.c		\
	devkit-power-source.h			devkit-power-source.c		\
	sysfs-utils.h				sysfs-utils.c			\
	main.c									\
	$(BUILT_SOURCES)

devkit_power_daemon_CPPFLAGS = 				\
	-I$(top_srcdir)/src				\
	-DG_LOG_DOMAIN=\"devkit-power-daemon\"		\
	$(DISABLE_DEPRECATED)				\
	$(AM_CPPFLAGS)

devkit_power_daemon_LDADD = 				\
	$(DBUS_GLIB_LIBS)				\
	$(POLKIT_DBUS_LIBS)				\
	$(DEVKIT_LIBS)

servicedir       = $(datadir)/dbus-1/system-services
service_in_files = org.freedesktop.DeviceKit.Power.service.in
service_DATA     = $(service_in_files:.service.in=.service)

$(service_DATA): $(service_in_files) Makefile
	@sed -e "s|\@libexecdir\@|$(libexecdir)|" $< > $@

dbusconfdir = $(sysconfdir)/dbus-1/system.d
dbusconf_in_files = org.freedesktop.DeviceKit.Power.conf.in
dbusconf_DATA = $(dbusconf_in_files:.conf.in=.conf)

$(dbusconf_DATA): $(dbusconf_in_files) Makefile
	cp $< $@

CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST = org.freedesktop.DeviceKit.Power.xml        	\
	     org.freedesktop.DeviceKit.Power.Source.xml		\
	     devkit-power-marshal.list				\
	     $(service_in_files)                        	\
	     $(dbusconf_in_files)

clean-local :
	rm -f *~ $(service_DATA) $(dbusconf_DATA)
