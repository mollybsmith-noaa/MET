## @start 1
## Makefile.am -- Process this file with automake to produce Makefile.in
## @end 1

MAINTAINERCLEANFILES	= Makefile.in

# Include the project definitions

include ${top_srcdir}/Make-include

# The program

bin_PROGRAMS    = mode_analysis
mode_analysis_SOURCES = mode_analysis.cc \
	config_to_att.cc
mode_analysis_CPPFLAGS = ${MET_CPPFLAGS}
mode_analysis_LDFLAGS = ${MET_LDFLAGS}
mode_analysis_LDADD = -lvx_stat_out \
	-lvx_statistics \
	-lvx_shapedata \
	-lvx_gsl_prob \
	-lvx_analysis_util \
	-lvx_shapedata \
	-lvx_util \
	$(PYTHON_LIBS) \
	-lvx_statistics \
	-lvx_data2d_factory \
	-lvx_data2d_nc_met \
	-lvx_data2d_nc_pinterp \
	$(PYTHON_LIBS) \
	-lvx_data2d_nccf \
	-lvx_data2d_grib $(GRIB2_LIBS) \
	-lvx_data2d \
	-lvx_nc_util \
	-lvx_regrid \
	-lvx_grid \
	-lvx_config \
	-lvx_gsl_prob \
	-lvx_cal \
	-lvx_util \
	-lvx_math \
	-lvx_color \
	-lvx_log \
	-lm -lnetcdf_c++4 -lnetcdf -lgsl -lgslcblas -lvx_util

EXTRA_DIST = config_to_att.h
