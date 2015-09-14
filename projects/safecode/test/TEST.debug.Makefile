##===- TEST.debug.Makefile ---------------------------------*- Makefile -*-===##
#
# This test runs performance experiments using SAFECode as a debugging tool.
#
##===----------------------------------------------------------------------===##

include $(PROJ_OBJ_ROOT)/Makefile.common

EXTRA_LOPT_OPTIONS :=

ifdef DEBUG_SAFECODE
#CFLAGS := -g -O0 -fno-strict-aliasing -fno-merge-constants
LLCFLAGS := -disable-fp-elim
LLVMLDFLAGS := -disable-opt
OPTZN_PASSES := -mem2reg -simplifycfg -adce

WHOLE_PROGRAM_BC_SUFFIX := linked.rbc
else
#CFLAGS := -g -O2 -fno-strict-aliasing -fno-merge-constants
OPTZN_PASSES := -std-compile-opts

WHOLE_PROGRAM_BC_SUFFIX := llvm.bc
endif

CURDIR  := $(shell cd .; pwd)
PROGDIR := $(shell cd $(LLVM_SRC_ROOT)/projects/test-suite; pwd)/
RELDIR  := $(subst $(PROGDIR),,$(CURDIR))
GCCLD    = $(LLVM_OBJ_ROOT)/$(CONFIGURATION)/bin/gccld
SCOPTS  := -terminate -check-every-gep-use -rewrite-oob
SCOPTS2 := -pa=apa
#SCOPTS2 := -pa=multi
#SCOPTS2 := -dpchecks
WATCHDOG := $(LLVM_OBJ_ROOT)/projects/safecode/$(CONFIGURATION)/bin/watchdog
SC       := $(RUNTOOLSAFELY) $(WATCHDOG) $(LLVM_OBJ_ROOT)/projects/safecode/$(CONFIGURATION)/bin/sc
CLANGBIN := $(LLVM_OBJ_ROOT)/projects/safecode/$(CONFIGURATION)/bin/clang
CLANGCPPBIN := $(LLVM_OBJ_ROOT)/projects/safecode/$(CONFIGURATION)/bin/clang
CLANG    = $(RUNTOOLSAFELY) $(WATCHDOG) $(CLANGBIN)
CLANGXX  = $(RUNTOOLSAFELY) $(WATCHDOG) $(CLANGCPPBIN)

LDFLAGS += -L$(PROJECT_DIR)/$(CONFIGURATION)/lib

# Bits of runtime to improve analysis
PA_PRE_RT_BC := $(POOLALLOC_OBJDIR)/$(CONFIGURATION)/lib/libpa_pre_rt.bca

SC_RT := libsc_dbg_rt libpoolalloc_bitmap libgdtoa

ifdef DEBUG_SAFECODE
PA_RT_O := $(addprefix $(PROJECT_DIR)/Debug/lib/,$(addsuffix .a,$(SC_RT)))
else
PA_RT_BC := $(addprefix $(PROJECT_DIR)/$(CONFIGURATION)/lib/,$(addsuffix .bca,$(SC_RT)))
endif

# SC_STATS - Run opt with the -stats and -time-passes options, capturing the
# output to a file.
SC_STATS = $(SC) -stats -time-passes -info-output-file=$(CURDIR)/$@.info

# Library for DSA
DSA_SO := $(POOLALLOC_OBJDIR)/$(CONFIGURATION)/lib/libLLVMDataStructure$(SHLIBEXT)

# Pre processing library for DSA
ASSIST_SO := $(POOLALLOC_OBJDIR)/$(CONFIGURATION)/lib/libAssistDS$(SHLIBEXT)

PRE_SC_OPT_FLAGS = -load $(DSA_SO) -load $(ASSIST_SO) -mem2reg -instnamer -internalize -indclone -funcspec -ipsccp -deadargelim -instcombine -globaldce -licm

#EXTRA_LINKTIME_OPT_FLAGS = $(EXTRA_LOPT_OPTIONS) 

ifeq ($(OS),Darwin)
LDFLAGS += -lpthread
else
LDFLAGS += -lrt -lpthread
endif

#
# When compiling on Linux, statically link in libstdc++.
# For other platforms, link it in normally.
#
ifeq ($(OS),Linux)
STATICFLAGS := -static
else
STATICFLAGS :=
endif

# DEBUGGING
#   o) Don't add -g to CFLAGS, CXXFLAGS, or CPPFLAGS; these are used by the
#      rules to compile code with llvm-gcc and enable LLVM debug information;
#      this, in turn, causes test cases to fail unnecessairly.
#CBECFLAGS += -g

SCObjs    := $(sort $(addsuffix .sc.o, $(notdir $(basename $(Source)))))
SCObjects := $(addprefix Output/,$(SCObjs))

.PRECIOUS: $(SCObjects)

#
# This rule compiles a single object file with SAFECode Clang
#
$(PROGRAMS_TO_TEST:%=Output/%.sc.o): \
Output/%.sc.o: %.c $(CLANG)
	-$(CLANG) -g -fmemsafety -Xclang -print-stats -o $@ $< $(LDFLAGS) 2>&1 > $@.out

$(PROGRAMS_TO_TEST:%=Output/%.sc.o): \
Output/%.sc.o: %.cpp $(CLANGXX)
	-$(CLANGXX) -g -fmemsafety -o $@ $< $(LDFLAGS) 2>&1 > $@.out

ifndef PROGRAMS_HAVE_CUSTOM_RUN_RULES
ifeq ($(USE_LTO),yes)
$(PROGRAMS_TO_TEST:%=Output/%.safecode): \
Output/%.safecode: $(addprefix $(PROJ_SRC_DIR)/,$(Source))
	-$(CLANG) -O2 -g -fmemsafety -Xclang -print-stats -use-gold-plugin -flto $(CPPFLAGS) $(CXXFLAGS) $(CFLAGS) $(addprefix $(PROJ_SRC_DIR)/,$(Source)) $(LDFLAGS) -o $@
else
$(PROGRAMS_TO_TEST:%=Output/%.safecode): \
Output/%.safecode: $(addprefix $(PROJ_SRC_DIR)/,$(Source))
	-$(CLANG) -O2 -g -fmemsafety -Xclang -print-stats $(CPPFLAGS) $(CXXFLAGS) $(CFLAGS) $(addprefix $(PROJ_SRC_DIR)/,$(Source)) $(LDFLAGS) -o $@
endif
else
$(PROGRAMS_TO_TEST:%=Output/%.safecode): \
Output/%.safecode: $(Source)
	-$(CLANG) -O2 -g -fmemsafety $(CPPFLAGS) $(CXXFLAGS) $(CFLAGS) $(Source) $(LDFLAGS) -o $@
endif
##############################################################################
# Rules for running executables and generating reports
##############################################################################

ifndef PROGRAMS_HAVE_CUSTOM_RUN_RULES

#
# This rule runs the generated executable, generating timing information, for
# normal test programs
#
$(PROGRAMS_TO_TEST:%=Output/%.safecode.out-llc): \
Output/%.safecode.out-llc: Output/%.safecode
	-$(RUNSAFELY) $(STDIN_FILENAME) $@ $(WATCHDOG) $< $(RUN_OPTIONS)

else

#
# This rule runs the generated executable, generating timing information, for
# SPEC
#
$(PROGRAMS_TO_TEST:%=Output/%.safecode.out-llc): \
Output/%.safecode.out-llc: Output/%.safecode
	-$(SPEC_SANDBOX) safecodecbe-$(RUN_TYPE) $@ $(REF_IN_DIR) \
             $(RUNSAFELY) $(STDIN_FILENAME) $(STDOUT_FILENAME) \
                  $(WATCHDOG) ../../$< $(RUN_OPTIONS)
	-(cd Output/safecodecbe-$(RUN_TYPE); cat $(LOCAL_OUTPUTS)) > $@
	-cp Output/safecodecbe-$(RUN_TYPE)/$(STDOUT_FILENAME).time $@.time

endif


# This rule diffs the post-poolallocated version to make sure we didn't break
# the program!
$(PROGRAMS_TO_TEST:%=Output/%.safecode.diff-llc): \
Output/%.safecode.diff-llc: Output/%.out-nat Output/%.safecode.out-llc
	@cp Output/$*.out-nat Output/$*.safecode.out-nat
	-$(DIFFPROG) llc $*.safecode $(HIDEDIFF)

# This rule wraps everything together to build the actual output the report is
# generated from.
$(PROGRAMS_TO_TEST:%=Output/%.$(TEST).report.txt): \
Output/%.$(TEST).report.txt: Output/%.out-nat                \
                             Output/%.safecode.diff-llc     \
                             Output/%.LOC.txt
	@echo > $@
	@echo ">>> ========= " \'$*\' Program >> $@
	@-if test -f Output/$*.out-nat; then \
	  printf "GCC-RUN-TIME: " >> $@;\
	  grep "^user" Output/$*.out-nat.time >> $@;\
        fi
	@-if test -f Output/$*.safecode.diff-llc; then \
	  printf "CBE-RUN-TIME-SAFECODE: " >> $@;\
	  grep "^user" Output/$*.safecode.out-llc.time >> $@;\
	fi
	printf "LOC: " >> $@
	cat Output/$*.LOC.txt >> $@
	#@cat Output/$*.$(TEST).bc.info >> $@

$(PROGRAMS_TO_TEST:%=test.$(TEST).%): \
test.$(TEST).%: Output/%.$(TEST).report.txt
	@echo "---------------------------------------------------------------"
	@echo ">>> ========= '$(RELDIR)/$*' Program"
	@echo "---------------------------------------------------------------"
	@cat $<

REPORT_DEPENDENCIES := $(PA_RT_O) $(CLANGBIN) $(PROGRAMS_TO_TEST:%=Output/%.llvm.bc)
