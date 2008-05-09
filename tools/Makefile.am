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
	$(GLIB_CFLAGS)

BUILT_SOURCES =							\
	devkit-power-daemon-glue.h				\
	devkit-power-marshal.h		devkit-power-marshal.c

devkit-power-marshal.h: $(top_srcdir)/src/devkit-power-marshal.list
	glib-genmarshal $< --prefix=devkit_power_marshal --header > $@

devkit-power-marshal.c: $(top_srcdir)/src/devkit-power-marshal.list
	echo "#include \"devkit-power-marshal.h\"" > $@ && glib-genmarshal $< --prefix=devkit_power_marshal --body >> $@

devkit-power-daemon-glue.h: $(top_srcdir)/src/org.freedesktop.DeviceKit.Power.xml Makefile.am
	dbus-binding-tool --prefix=devkit_power_daemon --mode=glib-client --output=devkit-power-daemon-glue.h $(top_srcdir)/src/org.freedesktop.DeviceKit.Power.xml

bin_PROGRAMS = devkit-power

devkit_power_SOURCES = 					\
				devkit-power.c		\
	$(BUILT_SOURCES)


devkit_power_CPPFLAGS = 				\
	-DG_LOG_DOMAIN=\"devkit-power\"		        \
	$(DISABLE_DEPRECATED)				\
	$(AM_CPPFLAGS)

devkit_power_LDADD = 					\
	$(DBUS_GLIB_LIBS)				\
	$(POLKIT_DBUS_LIBS)

CLEANFILES = $(BUILT_SOURCES)

clean-local :
	rm -f *~ $(service_DATA) $(dbusconf_DATA)
