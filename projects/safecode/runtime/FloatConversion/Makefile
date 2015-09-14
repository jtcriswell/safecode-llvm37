##===- runtime/FloatConversion/Makefile -----------------------------------===##

#
# Indicate where we are relative to the top of the source tree.
#
LEVEL=../../../..

LIBRARYNAME=gdtoa

BUILT_SOURCES = arith.h gd_qnan.h

SOURCES = dmisc.c dtoa.c gdtoa.c gmisc.c hdtoa.c ldtoa.c misc.c

#
# Include Makefile.common so we know what to do.
#
include $(LEVEL)/projects/safecode/Makefile.common

CFLAGS   += -I$(PROJ_OBJ_DIR)
CPPFLAGS += -I$(PROJ_OBJ_DIR)
CXXFLAGS += -I$(PROJ_OBJ_DIR)

clean::
	$(Verb) rm -f arithchk gd_qnan arith.h gd_qnan.h

arithchk: $(PROJ_SRC_DIR)/arithchk.c
	$(Echo) "Compiling arithchk"
	$(Verb) $(CC) $(CFLAGS) $< -o $@

arith.h: arithchk
	$(Echo) "Generating arith.h with arithchk"
	$(Verb) ./arithchk > $@

gd_qnan: arith.h $(PROJ_SRC_DIR)/qnan.c
	$(Echo) "Compiling gd_qnan"
	$(Verb) $(CC) $(CFLAGS) -I$(PROJ_OBJ_DIR) $(PROJ_SRC_DIR)/qnan.c -o $@

gd_qnan.h: gd_qnan
	$(Echo) "Generating gd_qnan.h with gd_qnan"
	$(Verb) ./gd_qnan > $@
