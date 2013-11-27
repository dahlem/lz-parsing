# Copyright (C) 2013 Dominik Dahlem <Dominik.Dahlem@gmail.com>
#  
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without 
# modifications, as long as this notice is preserved.
# 
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
ACLOCAL_AMFLAGS = -I m4


SUBDIRS = m4 src doc

EXTRA_DIST = \
	README \
	$(srcdir)/m4 \
	$(srcdir)/mk


include mk/doxygen.am


DISTCLEANFILES = configure
MAINTAINERCLEANFILES = "Makefile.in semantic.cache"
MOSTLYCLEANFILES = ${DX_CLEANFILES}

.PHONY: doxygen-run doxygen-doc $(DX_PS_GOAL) $(DX_PDF_GOAL)
