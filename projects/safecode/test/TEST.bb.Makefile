##===- TEST.debug.Makefile ---------------------------------*- Makefile -*-===##
#
# This test runs performance experiments comparing  SAFECode with and without 
# Baggy Bounds checking .
#
##===----------------------------------------------------------------------===##

include $(PROJ_OBJ_ROOT)/Makefile.common

EXTRA_LOPT_OPTIONS :=

ifdef DEBUG_SAFECODE
CFLAGS := -g -O0 -fno-strict-aliasing -fno-merge-constants
LLCFLAGS := -disable-fp-elim
LLVMLDFLAGS := -disable-opt
OPTZN_PASSES := -mem2reg -simplifycfg -adce

WHOLE_PROGRAM_BC_SUFFIX := linked.rbc
else
CFLAGS := -g -O2 -fno-strict-aliasing -fno-merge-constants
OPTZN_PASSES := -std-compile-opts

WHOLE_PROGRAM_BC_SUFFIX := llvm.bc
endif

CURDIR  := $(shell cd .; pwd)
PROGDIR := $(shell cd $(LLVM_SRC_ROOT)/projects/test-suite; pwd)/
RELDIR  := $(subst $(PROGDIR),,$(CURDIR))
GCCLD    = $(LLVM_OBJ_ROOT)/$(CONFIGURATION)/bin/gccld
SCOPTS  := -terminate -check-every-gep-use -rewrite-oob
SCOPTS2 := -poolalloc-force-simple-pool-init -pa=apa
#SCOPTS2 := -pa=multi
#SCOPTS2 := -dpchecks
WATCHDOG := $(LLVM_OBJ_ROOT)/projects/safecode/$(CONFIGURATION)/bin/watchdog
SC       := $(RUNTOOLSAFELY) $(WATCHDOG) $(LLVM_OBJ_ROOT)/projects/safecode/$(CONFIGURATION)/bin/sc

# Pool allocator pass shared object
PA_SO    := $(PROJECT_DIR)/$(CONFIGURATION)/lib/libaddchecks.a

# Bits of runtime to improve analysis
PA_PRE_RT_BC := $(POOLALLOC_OBJDIR)/$(CONFIGURATION)/lib/libpa_pre_rt.bca

SC_RT := libsc_dbg_rt libpoolalloc_bitmap
BB_RT := libsc_bb_rt

ifdef DEBUG_SAFECODE
PA_RT_O := $(addprefix $(PROJECT_DIR)/Debug/lib/,$(addsuffix .a,$(SC_RT)))
else
PA_RT_BC := $(addprefix $(PROJECT_DIR)/$(CONFIGURATION)/lib/,$(addsuffix .bca,$(SC_RT)))
endif

ifdef DEBUG_SAFECODE
BB_RT_O := $(addprefix $(PROJECT_DIR)/Debug/lib/,$(addsuffix .a,$(BB_RT)))
else
BB_RT_BC := $(addprefix $(PROJECT_DIR)/$(CONFIGURATION)/lib/,$(addsuffix .bca,$(BB_RT)))
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

#
# This rule links in part of the SAFECode run-time with the original program.
#
$(PROGRAMS_TO_TEST:%=Output/%.presc.bc): \
Output/%.presc.bc: Output/%.$(WHOLE_PROGRAM_BC_SUFFIX) $(LOPT) $(PA_PRE_RT_BC)
	-@rm -f $(CURDIR)/$@.info
	-$(LLVMLD) $(LLVMLDFLAGS) -link-as-library -o $@.paprert.bc $< $(PA_PRE_RT_BC) 2>&1 > $@.out
	-$(LOPT) $(PRE_SC_OPT_FLAGS) $@.paprert.bc -f -o $@ 2>&1 > $@.out

#
# Create a SAFECode executable without pointer rewriting.
#
$(PROGRAMS_TO_TEST:%=Output/%.noOOB.bc): \
Output/%.noOOB.bc: Output/%.presc.bc $(LOPT) $(PA_RT_BC)
	-@rm -f $(CURDIR)/$@.info
	-$(SC_STATS) $(SCOPTS) $< -f -o $@.noOOB 2>&1 > $@.out
	-$(LLVMLD) $(LLVMLDFLAGS) -o $@.noOOB.ld $@.noOOB $(PA_RT_BC) 2>&1 > $@.out
	-$(LOPT) $(OPTZN_PASSES) $@.noOOB.ld.bc -o $@ -f 2>&1    >> $@.out

#
# Create a SAFECode executable with pointer rewriting.
#
$(PROGRAMS_TO_TEST:%=Output/%.safecode.bc): \
Output/%.safecode.bc: Output/%.presc.bc $(LOPT) $(PA_RT_BC)
	-@rm -f $(CURDIR)/$@.info
	-$(SC_STATS) $(SCOPTS) $(SCOPTS2) $< -f -o $@.sc 2>&1 > $@.out
	-$(LLVMLD) $(LLVMLDFLAGS) -o $@.sc.ld $@.sc $(PA_RT_BC) 2>&1 > $@.out
	-$(LOPT) $(OPTZN_PASSES) $@.sc.ld.bc -o $@ -f 2>&1    >> $@.out

#
# Create a SAFECode executable using Baggy Bounds.
#
$(PROGRAMS_TO_TEST:%=Output/%.bb.bc): \
Output/%.bb.bc: Output/%.presc.bc $(LOPT) $(BB_RT_BC)
	-@rm -f $(CURDIR)/$@.info
	-$(SC_STATS) $(SCOPTS) $(SCOPTS2) -runtime=RUNTIME_BB $< -f -o $@.bb 2>&1 > $@.out
	-$(LLVMLD) $(LLVMLDFLAGS) -o $@.bb.ld $@.bb $(BB_RT_BC) 2>&1 > $@.out
	-$(LOPT) $(OPTZN_PASSES) $@.bb.ld.bc -o $@ -f 2>&1    >> $@.out

#
# These rules compile the new .bc file into a .c file using llc
#
$(PROGRAMS_TO_TEST:%=Output/%.safecode.s): \
Output/%.safecode.s: Output/%.safecode.bc $(LLC)
	-$(LLC) $(LLCFLAGS) -f $< -o $@

$(PROGRAMS_TO_TEST:%=Output/%.bb.s): \
Output/%.bb.s: Output/%.bb.bc $(LLC)
	-$(LLC) $(LLCFLAGS) -f $< -o $@

$(PROGRAMS_TO_TEST:%=Output/%.noOOB.s): \
Output/%.noOOB.s: Output/%.noOOB.bc $(LLC)
	-$(LLC) $(LLCFLAGS) -f $< -o $@

$(PROGRAMS_TO_TEST:%=Output/%.safecode.cbe.c): \
Output/%.safecode.cbe.c: Output/%.safecode.bc $(LLC)
	-$(LLC) -march=c $(LLCFLAGS) -f $< -o $@

$(PROGRAMS_TO_TEST:%=Output/%.bb.cbe.c): \
Output/%.bb.cbe.c: Output/%.bb.bc $(LLC)
	-$(LLC) -march=c $(LLCFLAGS) -f $< -o $@

$(PROGRAMS_TO_TEST:%=Output/%.noOOB.cbe.c): \
Output/%.noOOB.cbe.c: Output/%.noOOB.bc $(LLC)
	-$(LLC) -march=c $(LLCFLAGS) -f $< -o $@

#
# These rules compile the CBE .c file into a final executable
#
ifdef SC_USECBE
$(PROGRAMS_TO_TEST:%=Output/%.safecode): \
Output/%.safecode: Output/%.safecode.cbe.c $(PA_RT_O)
	-$(LLVMGCC) $(CBECFLAGS) $(CFLAGS) $< $(LLCLIBS) $(PA_RT_O) $(LDFLAGS) -o $@ $(STATICFLAGS) -lstdc++

$(PROGRAMS_TO_TEST:%=Output/%.bb): \
Output/%.bb: Output/%.bb.cbe.c $(BB_RT_O)
	-$(LLVMGCC) $(CBECFLAGS) $(CFLAGS) $< $(LLCLIBS) $(BB_RT_O) $(LDFLAGS) -o $@ $(STATICFLAGS) -lstdc++

$(PROGRAMS_TO_TEST:%=Output/%.noOOB): \
Output/%.noOOB: Output/%.noOOB.cbe.c
	-$(LLVMGCC) $(CBECFLAGS) $(CFLAGS) $< $(LLCLIBS) $(LDFLAGS) -o $@ $(STATICFLAGS) -lstdc++
else
$(PROGRAMS_TO_TEST:%=Output/%.safecode): \
Output/%.safecode: Output/%.safecode.s $(PA_RT_O)
	-$(LLVMGCC) $(CFLAGS) $< $(LLCLIBS) $(PA_RT_O) $(LDFLAGS) -o $@ $(STATICFLAGS) -lstdc++

$(PROGRAMS_TO_TEST:%=Output/%.bb): \
Output/%.bb: Output/%.bb.s $(BB_RT_O)
	-$(LLVMGCC) $(CFLAGS) $< $(LLCLIBS) $(BB_RT_O) $(LDFLAGS) -o $@ $(STATICFLAGS) -lstdc++

$(PROGRAMS_TO_TEST:%=Output/%.noOOB): \
Output/%.noOOB: Output/%.noOOB.s
	-$(LLVMGCC) $(CFLAGS) $< $(LLCLIBS) $(PA_RT_O) $(LDFLAGS) -o $@ $(STATICFLAGS) -lstdc++
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

$(PROGRAMS_TO_TEST:%=Output/%.bb.out-llc): \
Output/%.bb.out-llc: Output/%.bb
	-$(RUNSAFELY) $(STDIN_FILENAME) $@ $(WATCHDOG) $< $(RUN_OPTIONS)

$(PROGRAMS_TO_TEST:%=Output/%.noOOB.out-llc): \
Output/%.noOOB.out-llc: Output/%.noOOB
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

$(PROGRAMS_TO_TEST:%=Output/%.bb.out-llc): \
Output/%.bb.out-llc: Output/%.bb
	-$(SPEC_SANDBOX) bbcbe-$(RUN_TYPE) $@ $(REF_IN_DIR) \
             $(RUNSAFELY) $(STDIN_FILENAME) $(STDOUT_FILENAME) \
                  $(WATCHDOG) ../../$< $(RUN_OPTIONS)
	-(cd Output/bbcbe-$(RUN_TYPE); cat $(LOCAL_OUTPUTS)) > $@
	-cp Output/bbcbe-$(RUN_TYPE)/$(STDOUT_FILENAME).time $@.time

$(PROGRAMS_TO_TEST:%=Output/%.noOOB.out-llc): \
Output/%.noOOB.out-llc: Output/%.noOOB
	-$(SPEC_SANDBOX) noOOBcbe-$(RUN_TYPE) $@ $(REF_IN_DIR) \
             $(RUNSAFELY) $(STDIN_FILENAME) $(STDOUT_FILENAME) \
                  $(WATCHDOG) ../../$< $(RUN_OPTIONS)
	-(cd Output/noOOBcbe-$(RUN_TYPE); cat $(LOCAL_OUTPUTS)) > $@
	-cp Output/noOOBcbe-$(RUN_TYPE)/$(STDOUT_FILENAME).time $@.time

endif


# This rule diffs the post-poolallocated version to make sure we didn't break
# the program!
$(PROGRAMS_TO_TEST:%=Output/%.safecode.diff-llc): \
Output/%.safecode.diff-llc: Output/%.out-nat Output/%.safecode.out-llc
	@cp Output/$*.out-nat Output/$*.safecode.out-nat
	-$(DIFFPROG) llc $*.safecode $(HIDEDIFF)

$(PROGRAMS_TO_TEST:%=Output/%.bb.diff-llc): \
Output/%.bb.diff-llc: Output/%.out-nat Output/%.bb.out-llc
	@cp Output/$*.out-nat Output/$*.bb.out-nat
	-$(DIFFPROG) llc $*.bb $(HIDEDIFF)

$(PROGRAMS_TO_TEST:%=Output/%.noOOB.diff-llc): \
Output/%.noOOB.diff-llc: Output/%.out-nat Output/%.noOOB.out-llc
	@cp Output/$*.out-nat Output/$*.noOOB.out-nat
	-$(DIFFPROG) llc $*.noOOB $(HIDEDIFF)


# This rule wraps everything together to build the actual output the report is
# generated from.
$(PROGRAMS_TO_TEST:%=Output/%.$(TEST).report.txt): \
Output/%.$(TEST).report.txt: Output/%.out-nat                \
                             Output/%.noOOB.diff-llc     \
                             Output/%.safecode.diff-llc     \
                             Output/%.bb.diff-llc     \
                             Output/%.LOC.txt
	@echo > $@
	@echo ">>> ========= " \'$*\' Program >> $@
	@-if test -f Output/$*.out-nat; then \
	  printf "GCC-RUN-TIME: " >> $@;\
	  grep "^user" Output/$*.out-nat.time >> $@;\
        fi
	@-if test -f Output/$*.noOOB.diff-llc; then \
	  printf "CBE-RUN-TIME-NORMAL: " >> $@;\
	  grep "^user" Output/$*.noOOB.out-llc.time >> $@;\
        fi
	@-if test -f Output/$*.safecode.diff-llc; then \
	  printf "CBE-RUN-TIME-SAFECODE: " >> $@;\
	  grep "^user" Output/$*.safecode.out-llc.time >> $@;\
	fi
	@-if test -f Output/$*.bb.diff-llc; then \
	  printf "CBE-RUN-TIME-BB: " >> $@;\
	  grep "^user" Output/$*.bb.out-llc.time >> $@;\
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

REPORT_DEPENDENCIES := $(PA_RT_O) $(PA_SO) $(PROGRAMS_TO_TEST:%=Output/%.llvm.bc) $(LLC) $(LOPT)
