## Makefile.am -- Process this file with automake to produce Makefile.in
## Copyright (C) 2000, 2006 Gary V. Vaughan
##
## This program is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 2, or (at your option)
## any later version.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program; if not, write to the Free Software
## Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
## MA 02110-1301, USA.

# SUBDIRS = include


## Example of how to Install outside of $(pkgdatadir)
## scriptsrootdir = $(prefix)/share/scripts
## pythonscriptsdir = ${scriptsrootdir}/python

pythonscriptsdir = $(pkgdatadir)/python

pythonscripts_DATA = \
	met_point_obs.py \
	read_ascii_numpy.py \
	read_ascii_numpy_grid.py \
	read_ascii_xarray.py \
	read_ascii_point.py \
	read_ascii_mpr.py \
	read_met_point_obs.py

EXTRA_DIST = ${pythonscripts_DATA}

MAINTAINERCLEANFILES 	= Makefile.in
