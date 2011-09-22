## Makefile.am for DirectFB/flux

INCLUDES = \
	-I$(top_builddir)/lib \
	-I$(top_builddir)/include \
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/lib \
	@libdirect_CFLAGS@

bin_PROGRAMS = \
	fluxcomp


libdirect =


fluxcomp_SOURCES = fluxcomp.cpp
fluxcomp_LDADD   = @libdirect_LIBS@
