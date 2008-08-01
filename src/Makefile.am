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

BUILT_SOURCES =							\
	dkp-daemon-glue.h					\
	dkp-source-glue.h					\
	dkp-marshal.h			dkp-marshal.c

dkp-marshal.h: dkp-marshal.list
	glib-genmarshal $< --prefix=dkp_marshal --header > $@

dkp-marshal.c: dkp-marshal.list
	echo "#include \"dkp-marshal.h\"" > $@ && glib-genmarshal $< --prefix=dkp_marshal --body >> $@

dkp-daemon-glue.h: org.freedesktop.DeviceKit.Power.xml Makefile.am
	dbus-binding-tool --prefix=dkp_daemon --mode=glib-server --output=dkp-daemon-glue.h org.freedesktop.DeviceKit.Power.xml

dkp-source-glue.h: org.freedesktop.DeviceKit.Power.Source.xml Makefile.am
	dbus-binding-tool --prefix=dkp_source --mode=glib-server --output=dkp-source-glue.h org.freedesktop.DeviceKit.Power.Source.xml

libexec_PROGRAMS = devkit-power-daemon

dbusifdir = $(datadir)/dbus-1/interfaces
dbusif_DATA = org.freedesktop.DeviceKit.Power.xml org.freedesktop.DeviceKit.Power.Source.xml

devkit_power_daemon_SOURCES = 					\
	dkp-daemon.h			dkp-daemon.c		\
	dkp-device.h			dkp-device.c		\
	dkp-source.h			dkp-source.c		\
	dkp-enum.h			dkp-enum.c		\
	sysfs-utils.h			sysfs-utils.c		\
	main.c							\
	$(BUILT_SOURCES)

devkit_power_daemon_CPPFLAGS = 					\
	-I$(top_srcdir)/src					\
	-DG_LOG_DOMAIN=\"dkp-daemon\"				\
	$(DISABLE_DEPRECATED)					\
	$(AM_CPPFLAGS)

devkit_power_daemon_LDADD = 					\
	$(DBUS_GLIB_LIBS)					\
	$(POLKIT_DBUS_LIBS)					\
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
	     dkp-marshal.list					\
	     $(service_in_files)                        	\
	     $(dbusconf_in_files)

clean-local :
	rm -f *~ $(service_DATA) $(dbusconf_DATA)

