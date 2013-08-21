#
# Copyright 1998 - 2007 Double Precision, Inc.  See COPYING for
# distribution information.

noinst_LIBRARIES=libafx.a
noinst_PROGRAMS=testafxpipe

AFXSRC=afxpipe.C
AFXINCLUDE=afx.h

libafx_a_SOURCES=$(AFXSRC) $(AFXINCLUDE)

testafxpipe_SOURCES=testafxpipe.C
testafxpipe_DEPENDENCIES=libafx.a
testafxpipe_LDADD=libafx.a

check-am:
	./testafxpipe <libafx.a | cmp - libafx.a
	./testafxpipe <config.h | cmp - config.h

