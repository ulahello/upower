INCLUDES = \
	$(GLIB_CFLAGS)						\
	$(DBUS_GLIB_CFLAGS)					\
	-I$(top_srcdir)						\
	-I$(top_srcdir)/devkit-power-gobject			\
	-DDKP_COMPILATION					\
	-DG_LOG_DOMAIN=\"devkit-power-gobject\"			\
	-DPACKAGE_DATA_DIR=\""$(datadir)"\"			\
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\"

-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = --add-include-path=$(srcdir)
INTROSPECTION_COMPILER_ARGS = --includedir=$(srcdir)

lib_LTLIBRARIES =						\
	libdevkit-power-gobject.la

libdevkit_power_includedir = $(includedir)/DeviceKit-power/devkit-power-gobject
libdevkit_power_include_HEADERS =				\
	devicekit-power.h					\
	dkp-version.h						\
	dkp-client.h						\
	dkp-device.h						\
	dkp-wakeups.h						\
	dkp-qos-obj.h						\
	dkp-history-obj.h					\
	dkp-stats-obj.h						\
	dkp-wakeups-obj.h					\
	dkp-enum.h

libdevkit_power_gobject_la_SOURCES =				\
	devicekit-power.h					\
	dkp-version.h						\
	dkp-client.c						\
	dkp-client.h						\
	dkp-device.c						\
	dkp-device.h						\
	dkp-wakeups.c						\
	dkp-wakeups.h						\
	dkp-qos-obj.c						\
	dkp-qos-obj.h						\
	dkp-history-obj.c					\
	dkp-history-obj.h					\
	dkp-stats-obj.c						\
	dkp-stats-obj.h						\
	dkp-wakeups-obj.c					\
	dkp-wakeups-obj.h					\
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

libdevkit_power_gobject_la_CFLAGS =				\
	$(WARNINGFLAGS_C)					\
	$(NULL)

EXTRA_DIST =							\
	dkp-version.h.in

CLEANFILES = $(BUILT_SOURCES)

if HAVE_INTROSPECTION
introspection_sources = $(libdevkit_power_gobject_la_SOURCES)

DeviceKitPowerGlib-1.0.gir: libdevkit-power-gobject.la
DeviceKitPowerGlib_1_0_gir_INCLUDES = GObject-2.0 Gio-2.0
DeviceKitPowerGlib_1_0_gir_CFLAGS = $(INCLUDES) -DI_KNOW_THE_DEVICEKIT_POWER_API_IS_SUBJECT_TO_CHANGE
DeviceKitPowerGlib_1_0_gir_LIBS = libdevkit-power-gobject.la
DeviceKitPowerGlib_1_0_gir_FILES = $(addprefix $(srcdir)/,$(introspection_sources))
INTROSPECTION_GIRS += DeviceKitPowerGlib-1.0.gir

girdir = $(datadir)/gir-1.0
dist_gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(libdir)/girepositry-1.0
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(dist_gir_DATA) $(typelib_DATA)
endif

clean-local:
	rm -f *~

