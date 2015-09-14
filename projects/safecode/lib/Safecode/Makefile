LEVEL = ../../../..

LIBRARYNAME=safecode

#
# Build shared libraries on all platforms except Cygwin and MingW (which do
# not support them).
#
ifneq ($(OS),Cygwin)
ifneq ($(OS),MingW)
SHARED_LIBRARY=1
endif
endif

include $(LEVEL)/projects/safecode/Makefile.common

ObjectsO := $(LibDir)/addchecks.o $(LibDir)/arrayboundcheck.o $(LibDir)/indirectcalls.o $(LibDir)/speculativechecking.o $(LibDir)/convert.o $(LibDir)/pointerchecks.o $(LibDir)/stackcheck.o $(LibDir)/cmspasses.o 
ObjectsLO := $(LibDir)/addchecks.o $(LibDir)/arrayboundcheck.o $(LibDir)/indirectcalls.o $(LibDir)/speculativechecking.o $(LibDir)/convert.o $(LibDir)/pointerchecks.o $(LibDir)/stackcheck.o $(LibDir)/cmspasses.o 
