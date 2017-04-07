# -*- Makefile -*-
#
#

AM_CPPFLAGS = \
	$(BOOST_CPPFLAGS) \
	$(MPI_CPPFLAGS) \
	-I/opt/local/Library/Frameworks/Python.framework/Versions/3.4/include/python3.4m

compdir = $(pkglibdir)
comp_LTLIBRARIES = libcrossSim.la
libcrossSim_la_SOURCES = \
	crossSim.cc \
	crossSim.h

EXTRA_DIST = \
    README 

libcrossSim_la_LDFLAGS = -module -avoid-version /opt/local/Library/Frameworks/Python.framework/Versions/3.4/lib/python3.4/config-3.4m/libpython3.4.a 

##########################################################################
##########################################################################
##########################################################################


