## @start 1
## Makefile.am -- Process this file with automake to produce Makefile.in
## @end 1

MAINTAINERCLEANFILES	= Makefile.in

# Include the project definitions

include ${top_srcdir}/Make-include

# The library

noinst_LIBRARIES = libvx_pointdata_python.a
libvx_pointdata_python_a_SOURCES = \
              pointdata_python.h pointdata_python.cc \
              pointdata_from_array.h pointdata_from_array.cc \
              python_pointdata.h python_pointdata.cc
libvx_pointdata_python_a_CPPFLAGS = ${MET_CPPFLAGS} -I../vx_python2_utils ${MET_PYTHON_CC} $(MET_PYTHON_LD)

