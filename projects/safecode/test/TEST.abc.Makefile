##===- safecode/test/TEST.abc.Makefile ---------------------*- Makefile -*-===##
#
# This test runs SAFECode on all of the Programs, producing some
# performance numbers and statistics.
#
##===----------------------------------------------------------------------===##

include $(PROJ_OBJ_ROOT)/Makefile.common

CFLAGS = -O3 -fno-strict-aliasing
ABCFLAGS = -include /home/vadve/criswell/Downloads/SAFECode/lib/poolcheck.c -I/home/vadve/criswell/Downloads/SAFECode/include

CURDIR  := $(shell cd .; pwd)
PROGDIR := $(shell cd $(LLVM_SRC_ROOT)/projects/test-suite; pwd)/
RELDIR  := $(subst $(PROGDIR),,$(CURDIR))
GCCLD   =  $(LLVM_OBJ_ROOT)/$(CONFIGURATION)/bin/gccld

# Pool allocator pass shared object
PA_SO    := /home/vadve/criswell/Downloads/SAFECode/bin/boundschecker

# Pool allocator runtime library
#PA_RT    := $(PROJECT_DIR)/lib/Bytecode/libpoolalloc_fl_rt.bc
#PA_RT_O  := $(PROJECT_DIR)/$(CONFIGURATION)/lib/libpoolalloc_splay_rt.bca
#PA_RT_O  := $(PROJECT_DIR)/$(CONFIGURATION)/lib/poolalloc_splay_rt.o
#PA_RT_O  := $(PROJECT_DIR)/$(CONFIGURATION)/lib/poolalloc_safe_rt.o
#PA_RT_O  := $(PROJECT_DIR)/Release/lib/poolalloc_rt.o
#PA_RT_O  := $(PROJECT_DIR)/lib/Release/poolalloc_fl_rt.o
PA_RT_O  := /home/vadve/criswell/Downloads/SAFECode/lib/libpoolalloc_splay_rt.a

# Command to run opt with the pool allocator pass loaded
OPT_SC := /home/vadve/criswell/Downloads/SAFECode/bin/boundschecker \
          -poolalloc-disable-alignopt \
          -poolalloc-usetd-resolve  \
          -budatastructures-update-from-globals \
          -poolalloc-force-simple-pool-init \
          -poolalloc-force-all-poolfrees \
          -poolalloc-add-pooldescs-globalstack-arrays \
          -poolalloc-add-pooldescs-collapsed \
          -poolalloc-heuristic=AllNodes \
          -poolalloc-keep-DSA-results

#OPT_SC := /home/vadve/criswell/Downloads/SAFECode/bin/boundschecker \
          #-poolalloc-force-simple-pool-init \
          #-poolalloc-force-all-poolfrees \
          #-poolalloc-add-pooldescs-globalstack-arrays \
          #-poolalloc-heuristic=AllNodes \
          #-poolalloc-keep-DSA-results

# OPT_SC_STATS - Run opt with the -stats and -time-passes options, capturing the
# output to a file.
OPT_SC_STATS = $(OPT_SC) -info-output-file=$(CURDIR)/$@.info -stats -time-passes

#OPTZN_PASSES := -globaldce -ipsccp -deadargelim -adce -instcombine -simplifycfg
OPTZN_PASSES :=


#
# This rule runs SAFECode on the .llvm.bc file to produce a new .bc
# file
#
$(PROGRAMS_TO_TEST:%=Output/%.$(TEST).bc): \
Output/%.$(TEST).bc: Output/%.llvm.bc $(PA_SO) $(LOPT)
	-@rm -f $(CURDIR)/$@.info
	-$(OPT_SC_STATS) $(OPTZN_PASSES) $< -o $@ -f 2>&1 > $@.out

$(PROGRAMS_TO_TEST:%=Output/%.nonsc.bc): \
Output/%.nonsc.bc: Output/%.llvm.bc $(LOPT)
	-@rm -f $(CURDIR)/$@.info
	-$(LOPT) $(OPTZN_PASSES) $< -o $@ -f 2>&1 > $@.out

#
# These rules compile the new .bc file into a .c file using CBE
#
$(PROGRAMS_TO_TEST:%=Output/%.abc.cbe.c): \
Output/%.abc.cbe.c: Output/%.$(TEST).bc $(LLC)
	-$(LLC) -march=c -f $< -o $@

$(PROGRAMS_TO_TEST:%=Output/%.nonsc.cbe.c): \
Output/%.nonsc.cbe.c: Output/%.nonsc.bc $(LLC)
	-$(LLC) -march=c -f $< -o $@

#
# These rules compile the CBE .c file into a final executable
#
$(PROGRAMS_TO_TEST:%=Output/%.abc.cbe): \
Output/%.abc.cbe: Output/%.abc.cbe.c $(PA_RT_O)
	-$(CC) $(CFLAGS) $(ABCFLAGS) $< $(LLCLIBS) $(LDFLAGS) $(PA_RT_O) -o $@

$(PROGRAMS_TO_TEST:%=Output/%.nonsc.cbe): \
Output/%.nonsc.cbe: Output/%.nonsc.cbe.c $(PA_RT_O)
	-$(CC) $(CFLAGS) $< $(LLCLIBS) $(LDFLAGS) -o $@



ifndef PROGRAMS_HAVE_CUSTOM_RUN_RULES

#
# This rule runs the generated executable, generating timing information, for
# normal test programs
#
$(PROGRAMS_TO_TEST:%=Output/%.abc.out-cbe): \
Output/%.abc.out-cbe: Output/%.abc.cbe
	-$(RUNSAFELY) $(STDIN_FILENAME) $@ $< $(RUN_OPTIONS)

$(PROGRAMS_TO_TEST:%=Output/%.nonsc.out-cbe): \
Output/%.nonsc.out-cbe: Output/%.nonsc.cbe
	-$(RUNSAFELY) $(STDIN_FILENAME) $@ $< $(RUN_OPTIONS)

else

#
# This rule runs the generated executable, generating timing information, for
# SPEC
#
$(PROGRAMS_TO_TEST:%=Output/%.abc.out-cbe): \
Output/%.abc.out-cbe: Output/%.abc.cbe
	-$(SPEC_SANDBOX) abccbe-$(RUN_TYPE) $@ $(REF_IN_DIR) \
             $(RUNSAFELY) $(STDIN_FILENAME) $(STDOUT_FILENAME) \
                  ../../$< $(RUN_OPTIONS)
	-(cd Output/abccbe-$(RUN_TYPE); cat $(LOCAL_OUTPUTS)) > $@
	-cp Output/abccbe-$(RUN_TYPE)/$(STDOUT_FILENAME).time $@.time

$(PROGRAMS_TO_TEST:%=Output/%.nonsc.out-cbe): \
Output/%.nonsc.out-cbe: Output/%.nonsc.cbe
	-$(SPEC_SANDBOX) nonsccbe-$(RUN_TYPE) $@ $(REF_IN_DIR) \
             $(RUNSAFELY) $(STDIN_FILENAME) $(STDOUT_FILENAME) \
                  ../../$< $(RUN_OPTIONS)
	-(cd Output/nonsccbe-$(RUN_TYPE); cat $(LOCAL_OUTPUTS)) > $@
	-cp Output/nonsccbe-$(RUN_TYPE)/$(STDOUT_FILENAME).time $@.time

endif


# This rule diffs the post-poolallocated version to make sure we didn't break
# the program!
$(PROGRAMS_TO_TEST:%=Output/%.abc.diff-cbe): \
Output/%.abc.diff-cbe: Output/%.out-nat Output/%.abc.out-cbe
	@cp Output/$*.out-nat Output/$*.abc.out-nat
	-$(DIFFPROG) cbe $*.abc $(HIDEDIFF)

$(PROGRAMS_TO_TEST:%=Output/%.nonsc.diff-cbe): \
Output/%.nonsc.diff-cbe: Output/%.out-nat Output/%.nonsc.out-cbe
	@cp Output/$*.out-nat Output/$*.nonsc.out-nat
	-$(DIFFPROG) cbe $*.nonsc $(HIDEDIFF)


# This rule wraps everything together to build the actual output the report is
# generated from.
$(PROGRAMS_TO_TEST:%=Output/%.$(TEST).report.txt): \
Output/%.$(TEST).report.txt: Output/%.out-nat                \
                             Output/%.nonsc.diff-cbe         \
                             Output/%.abc.diff-cbe     \
                             Output/%.LOC.txt
	@echo > $@
	@-if test -f Output/$*.nonsc.diff-cbe; then \
	  printf "GCC-RUN-TIME: " >> $@;\
	  grep "^program" Output/$*.out-nat.time >> $@;\
        fi
	@-if test -f Output/$*.nonsc.diff-cbe; then \
	  printf "CBE-RUN-TIME-NORMAL: " >> $@;\
	  grep "^program" Output/$*.nonsc.out-cbe.time >> $@;\
        fi
	@-if test -f Output/$*.abc.diff-cbe; then \
	  printf "CBE-RUN-TIME-SAFECODE: " >> $@;\
	  grep "^program" Output/$*.abc.out-cbe.time >> $@;\
	fi
	printf "LOC: " >> $@
	cat Output/$*.LOC.txt >> $@
	@cat Output/$*.$(TEST).bc.info >> $@

$(PROGRAMS_TO_TEST:%=test.$(TEST).%): \
test.$(TEST).%: Output/%.$(TEST).report.txt
	@echo "---------------------------------------------------------------"
	@echo ">>> ========= '$(RELDIR)/$*' Program"
	@echo "---------------------------------------------------------------"
	@cat $<

REPORT_DEPENDENCIES := $(PA_RT_O) $(PA_SO) $(PROGRAMS_TO_TEST:%=Output/%.llvm.bc) $(LLC) $(LOPT)
