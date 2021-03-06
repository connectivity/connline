
AM_MAKEFLAGS = --no-print-directory

includedir = @includedir@/connline

include_HEADERS = include/backend.h \
		include/connline.h \
		include/data.h \
		include/dbus.h \
		include/event.h \
		include/list.h \
		include/utils.h

noinst_HEADERS = include/private.h

local_headers = $(foreach file,$(include_HEADERS) $(noinst_HEADERS), \
					include/connline/$(notdir $(file)))

BUILT_SOURCES = $(local_headers)

CLEANFILES = $(BUILT_SOURCES)

plugindir = $(libdir)/connline

if MAINTAINER_MODE
build_plugindir = $(abs_top_srcdir)/plugins/.libs
else
build_plugindir = $(plugindir)
endif # MAINTAINER_MODE


lib_LTLIBRARIES = src/libconnline.la

src_libconnline_la_CPPFLAGS = -std=gnu99 -Wall -Werror -O2 \
			-U_FORTIFY_SOURCE  -D_FORTIFY_SOURCE=2 \
			-DCONNLINE_PLUGIN_DIR=\""$(build_plugindir)"\" \
			$(DBUS_CFLAGS) $(DEV_CFLAGS)

src_libconnline_la_LIBADD = $(DBUS_LIBS) -ldl

src_libconnline_la_SOURCES = src/backend.c \
			src/connline.c \
			src/dbus.c \
			src/event.c \
			src/list.c \
			src/plugin.c \
			src/utils.c

plugin_LTLIBRARIES =
plugin_objects =

plugin_cflags = -std=gnu99 -Wall -O2 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2 \
				$(DBUS_CFLAGS) $(DEV_CFLAGS)

plugin_ldflags = -no-undefined -module -avoid-version $(DBUS_LIBS)

if CONNLINE_EVENT_GLIB
plugin_LTLIBRARIES += plugins/event_glib.la
plugin_objects += $(plugins_event_glib_la_OBJECTS)
plugins_event_glib_la_CFLAGS = $(plugin_cflags) $(GLIB_CFLAGS)
plugins_event_glib_la_LDFLAGS = $(plugin_ldflags) $(GLIB_LIBS)
plugins_event_glib_la_SOURCES = plugins/glib.c
endif # CONNLINE_EVENT_GLIB

if CONNLINE_EVENT_EFL
plugin_LTLIBRARIES += plugins/event_efl.la
plugin_objects += $(plugins_event_efl_la_OBJECTS)
plugins_event_efl_la_CFLAGS = $(plugin_cflags) $(EFL_CFLAGS)
plugins_event_efl_la_LDFLAGS = $(plugin_ldflags) $(EFL_LIBS)
plugins_event_efl_la_SOURCES = plugins/efl.c
endif # CONNLINE_EVENT_EFL

if CONNLINE_EVENT_LIBEVENT
plugin_LTLIBRARIES += plugins/event_libevent.la
plugin_objects += $(plugins_event_libevent_la_OBJECTS)
plugins_event_libevent_la_CFLAGS = $(plugin_cflags) $(LIBEVENT_CFLAGS) $(GLIB_CFLAGS)
plugins_event_libevent_la_LDFLAGS = $(plugin_ldflags) $(LIBEVENT_LIBS) $(GLIB_LIBS)
plugins_event_libevent_la_SOURCES = plugins/libevent.c
endif # CONNLINE_EVENT_LIBEVENT

if CONNLINE_BACKEND_CONNMAN
plugin_LTLIBRARIES += plugins/backend_connman.la
plugin_objects += $(plugins_backend_connman_la_OBJECTS)
plugins_backend_connman_la_CFLAGS = $(plugin_cflags)
plugins_backend_connman_la_LDFLAGS = $(plugin_ldflags)
plugins_backend_connman_la_SOURCES = plugins/connman.c
endif # CONNLINE_BACKEND_CONNMAN

if CONNLINE_BACKEND_NM
plugin_LTLIBRARIES += plugins/backend_nm.la
plugin_objects += $(plugins_backend_nm_la_OBJECTS)
plugins_backend_nm_la_CFLAGS = $(plugin_cflags)
plugins_backend_nm_la_LDFLAGS = $(plugin_ldflags)
plugins_backend_nm_la_SOURCES = plugins/nm.c
endif # CONNLINE_BACKEND_NM

if CONNLINE_BACKEND_WICD
plugin_LTLIBRARIES += plugins/backend_wicd.la
plugin_objects += $(plugins_backend_wicd_la_OBJECTS)
plugins_backend_wicd_la_CFLAGS = $(plugin_cflags)
plugins_backend_wicd_la_LDFLAGS = $(plugin_ldflags)
plugins_backend_wicd_la_SOURCES = plugins/wicd.c
endif # CONNLINE_BACKEND_WICD

if TEST

test_cflags = -std=gnu99 -Wall -O2 -U_FORTIFY_SOURCE  -D_FORTIFY_SOURCE=2 \
				$(DBUS_CFLAGS) $(DEV_CFLAGS)

noinst_PROGRAMS =

if CONNLINE_EVENT_GLIB
noinst_PROGRAMS += test/glib_test

test_glib_test_CFLAGS = $(test_cflags) $(GLIB_CFLAGS)
test_glib_test_LDADD = $(GLIB_LIBS) src/libconnline.la
test_glib_test_SOURCES = test/glib_test.c
endif # CONNLINE_EVENT_GLIB

if CONNLINE_EVENT_EFL
noinst_PROGRAMS += test/efl_test

test_efl_test_CFLAGS = $(test_cflags) $(EFL_CFLAGS)
test_efl_test_LDADD = $(EFL_LIBS) src/libconnline.la
test_efl_test_SOURCES = test/efl_test.c
endif # CONNLINE_EVENT_EFL

if CONNLINE_EVENT_LIBEVENT
noinst_PROGRAMS += test/libevent_test

test_libevent_test_CFLAGS = $(test_cflags) $(LIBEVENT_CFLAGS)
test_libevent_test_LDADD = $(LIBEVENT_LIBS) src/libconnline.la
test_libevent_test_SOURCES = test/libevent_test.c
endif # CONNLINE_EVENT_LIBEVENT

endif # TEST

pkgconfigdir = $(libdir)/pkgconfig

pkgconfig_DATA = connline.pc

include/connline/%.h: $(abs_top_srcdir)/include/%.h
		$(AM_V_at)$(MKDIR_P) include/connline
		$(AM_V_GEN)$(LN_S) $< $@

clean-local:
	@$(RM) -rf include/connline
