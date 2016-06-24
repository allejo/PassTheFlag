lib_LTLIBRARIES = PassTheFlag.la

PassTheFlag_la_SOURCES = PassTheFlag.cpp
PassTheFlag_la_CPPFLAGS= -I$(top_srcdir)/include -I$(top_srcdir)/plugins/plugin_utils
PassTheFlag_la_LDFLAGS = -module -avoid-version -shared
PassTheFlag_la_LIBADD = $(top_builddir)/plugins/plugin_utils/libplugin_utils.la

AM_CPPFLAGS = $(CONF_CPPFLAGS)
AM_CFLAGS = $(CONF_CFLAGS)
AM_CXXFLAGS = $(CONF_CXXFLAGS)

EXTRA_DIST = \
	README.PassTheFlag.txt \
	README.md \
	PassTheFlag.def \
	PassTheFlag.sln \
	PassTheFlag.vcxproj

MAINTAINERCLEANFILES =	\
	Makefile.in
