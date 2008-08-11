INCLUDES = \
	$(GLIB_CFLAGS)						\
	$(DBUS_GLIB_CFLAGS)					\
	-I$(top_srcdir)/libdevkit-power				\
	-DPACKAGE_DATA_DIR=\""$(datadir)"\"			\
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\"

lib_LTLIBRARIES =						\
	libdevkit-power.la

libdevkit_power_includedir = $(includedir)/DeviceKit-power/dkp-gobject

libdevkit_power_include_HEADERS =				\
	dkp-object.h
	dkp-client.h						\
	dkp-history-obj.h					\
	dkp-client-device.h					\
	dkp-enum.h

libdevkit_power_la_SOURCES =					\
	dkp-debug.c						\
	dkp-debug.h						\
	dkp-client.c						\
	dkp-client.h						\
	dkp-client-device.c					\
	dkp-client-device.h					\
	dkp-history-obj.h					\
	dkp-history-obj.c					\
	dkp-enum.c						\
	dkp-enum.h						\
	dkp-object.c						\
	dkp-object.h

libdevkit_power_la_LIBADD =					\
	$(INTLLIBS)						\
	$(GLIB_LIBS)						\
	$(DBUS_GLIB_LIBS)

libdevkit_power_la_LDFLAGS =					\
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)	\
	-export-dynamic						\
	-no-undefined						\
	-export-symbols-regex '^dkp_.*'

clean-local:
	rm -f *~

CLEANFILES = $(BUILT_SOURCES)

