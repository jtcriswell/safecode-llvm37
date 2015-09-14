##===- lib/FloatConversion/Makefile -----------------*- Makefile -*-===##

#
# Indicate where we are relative to the top of the source tree.
#
LEVEL=../..

LIBRARYNAME=gdtoa

BYTECODE_LIBRARY = 1

BUILT_SOURCES = arith.h gd_qnan.h

SOURCES = dmisc.c dtoa.c gdtoa.c gmisc.c hdtoa.c ldtoa.c misc.c

#
# Include Makefile.common so we know what to do.
#
include $(LEVEL)/Makefile.common

SRCPATH=$(PROJ_SRC_ROOT)/runtime/FloatConversion
OBJPATH=$(PROJ_OBJ_ROOT)/runtime/FloatConversion

CFLAGS   += -I$(OBJPATH)
CPPFLAGS += -I$(OBJPATH)
CXXFLAGS += -I$(OBJPATH)

arith.h: $(SRCPATH)/arithchk.c
	$(CC) $(SRCPATH)/arithchk.c -o $(OBJPATH)/arithchk
	$(OBJPATH)/arithchk >$(OBJPATH)/arith.h
	rm -f $(OBJPATH)/arithchk

gd_qnan.h: arith.h $(SRCPATH)/qnan.c
	$(CC) $(SRCPATH)/qnan.c -I$(OBJPATH) -o $(OBJPATH)/gd_qnan
	$(OBJPATH)/gd_qnan >$(OBJPATH)/gd_qnan.h
	rm -f $(OBJPATH)/gd_qnan
