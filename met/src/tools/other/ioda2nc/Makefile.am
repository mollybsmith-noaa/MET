## @start 1
## Makefile.am -- Process this file with automake to produce Makefile.in
## @end 1

MAINTAINERCLEANFILES	= Makefile.in

# Include the project definitions

include ${top_srcdir}/Make-include

# The program

bin_PROGRAMS  = ioda2nc
ioda2nc_SOURCES = ioda2nc.cc \
	ioda2nc_conf_info.cc ioda2nc_conf_info.h
ioda2nc_CPPFLAGS = ${MET_CPPFLAGS}
ioda2nc_LDFLAGS = ${MET_LDFLAGS}
ioda2nc_LDADD = -lvx_stat_out \
	-lvx_statistics \
	-lvx_shapedata \
	-lvx_analysis_util \
	-lvx_data2d_factory \
	-lvx_data2d_nc_met \
	-lvx_data2d_grib $(GRIB2_LIBS) \
	-lvx_data2d_nc_pinterp \
	-lvx_data2d_nccf \
	$(PYTHON_LIBS) \
	-lvx_data2d \
	-lvx_nc_obs \
	-lvx_nc_util \
	-lvx_regrid \
	-lvx_grid \
	-lvx_pb_util \
	-lvx_summary \
	-lvx_config \
	-lvx_gsl_prob \
	-lvx_color \
	-lvx_util \
	-lvx_math \
	-lvx_cal \
	-lvx_log \
	$(PYTHON_LIBS) \
	-lm -lnetcdf_c++4 -lnetcdf -lgsl -lgslcblas $(BLIB_NAME) \
	$(FLIBS)

