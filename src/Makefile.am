## Process this file with automake to produce Makefile.in

INCLUDES = \
	-I$(top_builddir)/src -I$(top_srcdir)/src		\
	-DPACKAGE_LIBEXEC_DIR=\""$(libexecdir)"\"		\
	-DPACKAGE_SYSCONF_DIR=\""$(sysconfdir)"\"		\
	-DPACKAGE_DATA_DIR=\""$(datadir)"\"			\
	-DPACKAGE_BIN_DIR=\""$(bindir)"\"			\
	-DPACKAGE_LOCALSTATE_DIR=\""$(localstatedir)"\"		\
	-DPACKAGE_LOCALE_DIR=\""$(localedir)"\"			\
	-DPACKAGE_LIB_DIR=\""$(libdir)"\"			\
	-D_POSIX_PTHREAD_SEMANTICS -D_REENTRANT			\
	-DDKP_COMPILATION					\
	-DEGG_LOG_FILE=\""$(DKP_LOG_DIR)/DeviceKit-power"\"	\
	-DEGG_VERBOSE="\"DKP_VERBOSE\""				\
	-DEGG_LOGGING="\"DKP_LOGGING\""				\
	-DEGG_CONSOLE="\"DKP_CONSOLE\""				\
	-I$(top_srcdir)/devkit-power-gobject			\
	-I$(top_srcdir)						\
	$(GIO_CFLAGS)						\
	$(DBUS_GLIB_CFLAGS)					\
	$(POLKIT_DBUS_CFLAGS)					\
	$(DEVKIT_CFLAGS)					\
	$(GLIB_CFLAGS)

DEVKIT_POWER_LIBS = $(top_builddir)/devkit-power-gobject/libdevkit-power-gobject.la

BUILT_SOURCES =							\
	dkp-daemon-glue.h					\
	dkp-device-glue.h					\
	dkp-qos-glue.h						\
	dkp-wakeups-glue.h					\
	dkp-marshal.h						\
	dkp-marshal.c

dkp-marshal.h: dkp-marshal.list
	glib-genmarshal $< --prefix=dkp_marshal --header > $@

dkp-marshal.c: dkp-marshal.list
	echo "#include \"dkp-marshal.h\"" > $@ && glib-genmarshal $< --prefix=dkp_marshal --body >> $@

dkp-daemon-glue.h: org.freedesktop.DeviceKit.Power.xml Makefile.am
	dbus-binding-tool --prefix=dkp_daemon --mode=glib-server --output=dkp-daemon-glue.h org.freedesktop.DeviceKit.Power.xml

dkp-device-glue.h: org.freedesktop.DeviceKit.Power.Device.xml Makefile.am
	dbus-binding-tool --prefix=dkp_device --mode=glib-server --output=dkp-device-glue.h org.freedesktop.DeviceKit.Power.Device.xml

dkp-qos-glue.h: org.freedesktop.DeviceKit.Power.QoS.xml Makefile.am
	dbus-binding-tool --prefix=dkp_qos --mode=glib-server --output=dkp-qos-glue.h org.freedesktop.DeviceKit.Power.QoS.xml

dkp-wakeups-glue.h: org.freedesktop.DeviceKit.Power.Wakeups.xml Makefile.am
	dbus-binding-tool --prefix=dkp_wakeups --mode=glib-server --output=dkp-wakeups-glue.h org.freedesktop.DeviceKit.Power.Wakeups.xml

libexec_PROGRAMS = devkit-power-daemon

dbusifdir = $(datadir)/dbus-1/interfaces
dbusif_DATA = 							\
	org.freedesktop.DeviceKit.Power.xml 			\
	org.freedesktop.DeviceKit.Power.Device.xml 		\
	org.freedesktop.DeviceKit.Power.QoS.xml 		\
	org.freedesktop.DeviceKit.Power.Wakeups.xml

devkit_power_daemon_SOURCES = 					\
	egg-debug.c						\
	egg-debug.h						\
	dkp-polkit.h			dkp-polkit.c		\
	dkp-daemon.h			dkp-daemon.c		\
	dkp-device.h			dkp-device.c		\
	dkp-device-list.h		dkp-device-list.c	\
	dkp-device-supply.h		dkp-device-supply.c	\
	dkp-device-csr.h		dkp-device-csr.c	\
	dkp-device-wup.h		dkp-device-wup.c	\
	dkp-device-hid.h		dkp-device-hid.c	\
	dkp-input.h			dkp-input.c		\
	dkp-qos.h			dkp-qos.c		\
	dkp-wakeups.h			dkp-wakeups.c		\
	dkp-history.h			dkp-history.c		\
	sysfs-utils.h			sysfs-utils.c		\
	dkp-main.c						\
	$(BUILT_SOURCES)

devkit_power_daemon_CPPFLAGS = 					\
	-I$(top_srcdir)/src					\
	-DG_LOG_DOMAIN=\"dkp-daemon\"				\
	$(DISABLE_DEPRECATED)					\
	$(AM_CPPFLAGS)

devkit_power_daemon_LDADD = 					\
	-lm							\
	$(USB_LIBS)						\
	$(GIO_LIBS)						\
	$(DBUS_GLIB_LIBS)					\
	$(POLKIT_DBUS_LIBS)					\
	$(DEVKIT_POWER_LIBS)					\
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

udevrulesdir = $(sysconfdir)/udev/rules.d
udevrules_DATA = 						\
	95-devkit-power-wup.rules        			\
	95-devkit-power-hid.rules        			\
	95-devkit-power-csr.rules

install-data-hook:
	if test -w $(DESTDIR)$(prefix)/; then \
		mkdir -p $(DESTDIR)$(localstatedir)/lib/DeviceKit-power; \
	fi

CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST =							\
	org.freedesktop.DeviceKit.Power.xml        		\
	org.freedesktop.DeviceKit.Power.Device.xml		\
	org.freedesktop.DeviceKit.Power.QoS.xml			\
	org.freedesktop.DeviceKit.Power.Wakeups.xml		\
	dkp-marshal.list					\
	95-devkit-power-csr.rules				\
	95-devkit-power-hid.rules				\
	95-devkit-power-wup.rules				\
	$(service_in_files)                        		\
	$(dbusconf_in_files)

clean-local :
	rm -f *~ $(service_DATA) $(dbusconf_DATA)

