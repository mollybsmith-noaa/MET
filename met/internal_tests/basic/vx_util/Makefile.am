## @start 1
## Makefile.am -- Process this file with automake to produce Makefile.in
## @end 1

MAINTAINERCLEANFILES	= Makefile.in

# Include the project definitions

include ${top_srcdir}/Make-include

if ENABLE_PYTHON
LDFLAGS += -lvx_python3_utils
endif

# Test programs

noinst_PROGRAMS = test_command_line \
	test_data_plane \
	test_add_rows   \
	test_table_float \
	test_ascii_header

test_command_line_SOURCES = test_command_line.cc
test_command_line_CPPFLAGS = ${MET_CPPFLAGS}
test_command_line_LDFLAGS = -L. ${MET_LDFLAGS}
test_command_line_LDADD = -lvx_util \
	-lvx_log

if ENABLE_PYTHON
test_command_line_LDADD += -lvx_python3_utils $(MET_PYTHON_LD)
endif

test_data_plane_SOURCES = test_data_plane.cc
test_data_plane_CPPFLAGS = ${MET_CPPFLAGS}
test_data_plane_LDFLAGS = -L. ${MET_LDFLAGS}
test_data_plane_LDADD = -lvx_util \
	-lvx_config \
	-lvx_gsl_prob \
	-lvx_cal \
	-lvx_util \
	-lvx_math \
	-lvx_log \
	-lgsl -lgslcblas

if ENABLE_PYTHON
test_data_plane_LDADD += -lvx_python3_utils $(MET_PYTHON_LD)
endif

test_add_rows_SOURCES = test_add_rows.cc
test_add_rows_CPPFLAGS = ${MET_CPPFLAGS}
test_add_rows_LDFLAGS = -L. ${MET_LDFLAGS}
test_add_rows_LDADD = -lvx_util \
	-lvx_config \
	-lvx_gsl_prob \
	-lvx_cal \
	-lvx_util \
	-lvx_math \
	-lvx_log \
	-lgsl -lgslcblas

if ENABLE_PYTHON
test_add_rows_LDADD += -lvx_python3_utils $(MET_PYTHON_LD)
endif

test_table_float_SOURCES = test_table_float.cc
test_table_float_CPPFLAGS = ${MET_CPPFLAGS}
test_table_float_LDFLAGS = -L. ${MET_LDFLAGS}
test_table_float_LDADD = -lvx_util \
	-lvx_config \
	-lvx_gsl_prob \
	-lvx_cal \
	-lvx_util \
	-lvx_math \
	-lvx_log \
	-lgsl -lgslcblas


if ENABLE_PYTHON
test_table_float_LDADD += -lvx_python3_utils $(MET_PYTHON_LD)
endif

test_ascii_header_SOURCES = test_ascii_header.cc
test_ascii_header_CPPFLAGS = ${MET_CPPFLAGS}
test_ascii_header_LDFLAGS = -L. ${MET_LDFLAGS}
test_ascii_header_LDADD = -lvx_stat_out \
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
