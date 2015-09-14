LEVEL = ../../../..
LIBRARYNAME=sc_bb_rt
BYTECODE_LIBRARY=1

ifeq ($(OS),Linux)
CXX.Flags += -march=native
else
CXX.Flags += -march=nocona
endif

CXX.Flags += -fno-threadsafe-statics
include $(LEVEL)/projects/safecode/Makefile.common

# Always build optimized and debug versions
all:: $(LIBNAME_OBJO) $(LIBNAME_OBJG)
