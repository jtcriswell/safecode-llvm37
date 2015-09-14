LEVEL = ../../../..

LIBRARYNAME=abc

SOURCES := \
            ArrayBoundCheckDummy.cpp \
            ArrayBoundCheckLocal.cpp \
            #ArrayBoundCheckStruct.cpp
            #BreakConstantGEPs.cpp \
            #AffineExpressions.cpp \
            #BottomUpCallGraph.cpp

#ABCPreProcess.cpp ArrayBoundCheck.cpp 

CFLAGS   += -DOMEGASCRIPT=\"${PROJ_SRC_ROOT}/utils/omega.pl\"
CPPFLAGS += -DOMEGASCRIPT=\"${PROJ_SRC_ROOT}/utils/omega.pl\"
CXXFLAGS += -DOMEGASCRIPT=\"${PROJ_SRC_ROOT}/utils/omega.pl\"

include $(LEVEL)/projects/safecode/Makefile.common

