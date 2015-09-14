LEVEL=../../../..
LIBRARYNAME=softbound_rt

CFlags += -march=native -D__SOFTBOUNDCETS_TRIE -D__SOFTBOUNDCETS_SPATIAL_TEMPORAL
CXX.Flags += -march=native -D__SOFTBOUNDCETS_TRIE -D__SOFTBOUNDCETS_SPATIAL_TEMPORAL

ifeq ($(OS),Linux)
CXX.Flags += -march=native -D__SOFTBOUNDCETS_TRIE -D__SOFTBOUNDCETS_SPATIAL_TEMPORAL
else
CXX.Flags += -march=nocona -D__SOFTBOUNDCETS_TRIE -D__SOFTBOUNDCETS_SPATIAL_TEMPORAL
endif

CXX.Flags += -fno-threadsafe-statics
include $(LEVEL)/projects/safecode/Makefile.common

