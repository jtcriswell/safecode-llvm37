##===- safecode/test/TEST.valgrind.Makefile ----------------*- Makefile -*-===##
#
# This test runs both SAFECode and valgrind on all of the Programs and produces
# performance numbers and statistics.
#
##===----------------------------------------------------------------------===##

include $(PROJ_OBJ_ROOT)/Makefile.common

#
# Turn on debug information for use with the SAFECode tool
#
CFLAGS = -g -O2 -fno-strict-aliasing

CURDIR  := $(shell cd .; pwd)
PROGDIR := $(shell cd $(LLVM_SRC_ROOT)/projects/test-suite; pwd)/
RELDIR  := $(subst $(PROGDIR),,$(CURDIR))
GCCLD    = $(LLVM_OBJ_ROOT)/$(CONFIGURATION)/bin/gccld
WATCHDOG := $(LLVM_OBJ_ROOT)/projects/safecode/$(CONFIGURATION)/bin/watchdog
SC       := $(RUNTOOLSAFELY) $(WATCHDOG) $(LLVM_OBJ_ROOT)/projects/safecode/$(CONFIGURATION)/bin/sc -rewrite-oob
VALGRINDPROG := $(HOME)/local/bin/valgrind
VALGRIND = $(VALGRINDPROG) -q --log-file=vglog
#VALGRIND = $(VALGRINDPROG) -q --log-file=vglog --tool=exp-sgcheck

CLANGBIN := $(LLVM_OBJ_ROOT)/projects/safecode/$(CONFIGURATION)/bin/clang
CLANGCPPBIN := $(LLVM_OBJ_ROOT)/projects/safecode/$(CONFIGURATION)/bin/clang
CLANG    = $(RUNTOOLSAFELY) $(WATCHDOG) $(CLANGBIN)
CLANGXX  = $(RUNTOOLSAFELY) $(WATCHDOG) $(CLANGCPPBIN)

# Pool allocator runtime library
#PA_RT_O  := $(PROJECT_DIR)/$(CONFIGURATION)/lib/poolalloc_safe_rt.o
PA_RT_BC := libsc_dbg_rt.bca libpoolalloc_bitmap.bca 
PA_RT_BC := $(addprefix $(PROJECT_DIR)/$(CONFIGURATION)/lib/, $(PA_RT_BC))

# Pool System library for interacting with the system
#POOLSYSTEM := $(PROJECT_DIR)/$(CONFIGURATION)/lib/UserPoolSystem.o
POOLSYSTEM :=

# SC_STATS - Run opt with the -stats and -time-passes options, capturing the
# output to a file.
SC_STATS = $(SC) -stats -time-passes -info-output-file=$(CURDIR)/$@.info

#OPTZN_PASSES := -globaldce -ipsccp -deadargelim -adce -instcombine -simplifycfg
OPTZN_PASSES := -strip-debug -std-compile-opts

LDFLAGS += -use-gold-plugin -L$(PROJECT_DIR)/$(CONFIGURATION)/lib

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
$(PROGRAMS_TO_TEST:%=Output/%.safecode): \
Output/%.safecode: $(addprefix $(PROJ_SRC_DIR)/,$(Source))
	-$(CLANG) -O2 -g -fmemsafety -Xclang -print-stats $(CPPFLAGS) $(CXXFLAGS) $(CFLAGS) $(addprefix $(PROJ_SRC_DIR)/,$(Source)) $(LDFLAGS) -o $@
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

$(PROGRAMS_TO_TEST:%=Output/%.valgrind.out-llc): \
Output/%.valgrind.out-llc: Output/%.native
	-$(RUNSAFELY) $(STDIN_FILENAME) $@ $(WATCHDOG) $(VALGRIND) $< $(RUN_OPTIONS)

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

$(PROGRAMS_TO_TEST:%=Output/%.valgrind.out-llc): \
Output/%.valgrind.out-llc: Output/%.native
	-$(SPEC_SANDBOX) nonsccbe-$(RUN_TYPE) $@ $(REF_IN_DIR) \
             $(RUNSAFELY) $(STDIN_FILENAME) $(STDOUT_FILENAME) \
                  $(WATCHDOG) $(VALGRIND) ../../$< $(RUN_OPTIONS)
	-(cd Output/nonsccbe-$(RUN_TYPE); cat $(LOCAL_OUTPUTS)) > $@
	-cp Output/nonsccbe-$(RUN_TYPE)/$(STDOUT_FILENAME).time $@.time

endif


# This rule diffs the post-poolallocated version to make sure we didn't break
# the program!
$(PROGRAMS_TO_TEST:%=Output/%.safecode.diff-llc): \
Output/%.safecode.diff-llc: Output/%.out-nat Output/%.safecode.out-llc
	@cp Output/$*.out-nat Output/$*.safecode.out-nat
	-$(DIFFPROG) llc $*.safecode $(HIDEDIFF)

$(PROGRAMS_TO_TEST:%=Output/%.valgrind.diff-llc): \
Output/%.valgrind.diff-llc: Output/%.out-nat Output/%.valgrind.out-llc
	@cp Output/$*.out-nat Output/$*.valgrind.out-nat
	-$(DIFFPROG) llc $*.valgrind $(HIDEDIFF)


# This rule wraps everything together to build the actual output the report is
# generated from.
$(PROGRAMS_TO_TEST:%=Output/%.$(TEST).report.txt): \
Output/%.$(TEST).report.txt: Output/%.out-nat                \
                             Output/%.valgrind.diff-llc     \
                             Output/%.safecode.diff-llc     \
                             Output/%.LOC.txt
	@echo > $@
	@echo ">>> ========= " \'$*\' Program >> $@
	@-if test -f Output/$*.out-nat; then \
	  printf "GCC-RUN-TIME: " >> $@;\
	  grep "^user" Output/$*.out-nat.time >> $@;\
        fi
	@-if test -f Output/$*.valgrind.diff-llc; then \
	  printf "RUN-TIME-VALGRIND: " >> $@;\
	  grep "^user" Output/$*.valgrind.out-llc.time >> $@;\
	fi
	@-if test -f Output/$*.safecode.diff-llc; then \
	  printf "RUN-TIME-SAFECODE: " >> $@;\
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

REPORT_DEPENDENCIES := $(PA_RT_O) $(PROGRAMS_TO_TEST:%=Output/%.llvm.bc) $(LLC) $(LOPT)
