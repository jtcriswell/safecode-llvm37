LEVEL = ../../../..
LIBRARYNAME=sc_dbg_rt
#BYTECODE_LIBRARY=1

ifeq ($(OS),Linux)
CXX.Flags += -march=native
else
CXX.Flags += -march=nocona
endif

CXX.Flags += -fno-threadsafe-statics
include $(LEVEL)/projects/safecode/Makefile.common

