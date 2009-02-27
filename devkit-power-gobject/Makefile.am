INCLUDES = \
	$(GLIB_CFLAGS)						\
	$(DBUS_GLIB_CFLAGS)					\
	-I$(top_srcdir)						\
	-I$(top_srcdir)/devkit-power-gobject			\
	-DDKP_COMPILATION					\
	-DG_LOG_DOMAIN=\"devkit-power-gobject\"			\
	-DPACKAGE_DATA_DIR=\""$(datadir)"\"			\
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\"

lib_LTLIBRARIES =						\
	libdevkit-power-gobject.la

libdevkit_power_includedir = $(includedir)/DeviceKit-power/devkit-power-gobject
libdevkit_power_include_HEADERS =				\
	dkp-client.h						\
	dkp-device.h						\
	dkp-wakeups.h						\
	dkp-qos-obj.h						\
	dkp-history-obj.h					\
	dkp-wakeups-obj.h					\
	dkp-enum.h

libdevkit_power_gobject_la_SOURCES =				\
	devicekit-power.h					\
	dkp-client.c						\
	dkp-client.h						\
	dkp-device.c						\
	dkp-device.h						\
	dkp-wakeups.c						\
	dkp-wakeups.h						\
	dkp-qos-obj.h						\
	dkp-qos-obj.c						\
	dkp-history-obj.h					\
	dkp-history-obj.c					\
	dkp-stats-obj.h						\
	dkp-stats-obj.c						\
	dkp-wakeups-obj.h					\
	dkp-wakeups-obj.c					\
	dkp-enum.c						\
	dkp-enum.h

libdevkit_power_gobject_la_LIBADD =				\
	$(INTLLIBS)						\
	$(GLIB_LIBS)						\
	$(DBUS_GLIB_LIBS)

libdevkit_power_gobject_la_LDFLAGS =				\
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)	\
	-export-dynamic						\
	-no-undefined						\
	-export-symbols-regex '^dkp_.*'

clean-local:
	rm -f *~

CLEANFILES = $(BUILT_SOURCES)

