bin_PROGRAMS			+= fxtool/fxtool

fxtool_fxtool_CPPFLAGS	= \
	-I$(top_srcdir)/lib

fxtool_fxtool_SOURCES	=	\
	fxtool/fxtool.c

fxtool_fxtool_LDADD	= \
	$(MODULE_DEPS_LIBS) \
	$(TOOL_DEPS_LIBS)

EXTRA_DIST += fxtool/CMakeLists.txt

EXTRA_fxtool_fxtool_DEPENDENCIES	= lib/libsyslog-ng.la
