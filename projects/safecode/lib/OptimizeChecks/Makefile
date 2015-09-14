
LEVEL = ../../../..

LIBRARYNAME=optchecks

ifneq ($(OS),Cygwin)
ifneq ($(OS),MingW)
#SHARED_LIBRARY := 1
#LOADABLE_MODULE := 1
endif
endif

#SOURCES := OptimizeChecks.cpp MonotonicLoopOpt.cpp
SOURCES := OptimizeChecks.cpp GlobalRegisterOpt.cpp \
					 RemoveSlowChecks.cpp InlineFastChecks.cpp SafeLoadStoreOpts.cpp

include $(LEVEL)/projects/safecode/Makefile.common

