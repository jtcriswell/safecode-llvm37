##===- safecode/test/Makefile ------------------------------*- Makefile -*-===##
#
# Define the default test rule, which allows people to just type 'make test' in
# this directory.
#
##===----------------------------------------------------------------------===##

# The default target in this directory is the test:: target
test::

LEVEL = ..
include $(LEVEL)/Makefile.common

ifndef NO_STABLE_NUMBERS
STABLERUN := GET_STABLE_NUMBERS=1
else
STABLERUN :=
endif

ifndef NO_LARGE_SIZE
LARGESIZE := LARGE_PROBLEM_SIZE=1
else
LARGESIZE :=
endif

ifdef ENABLE_OPTIMIZED
OPTIMIZED := ENABLE_OPTIMIZED=1
else
OPTIMIZED :=
endif

RUNTIMELIMIT=3600

PROFILING_DAT_DIR := $(shell pwd)/data
PROFILING_RESULT_DIR := $(shell pwd)/data/profiling

LARGE_PROBLEM_SIZE_DIRS :=


LARGE_PROBLEM_SIZE_DIRS := \
   $(addprefix $(TEST_OBJDIR)/,$(LARGE_PROBLEM_SIZE_DIRS))

#DEBUG_SAFECODE := DEBUG_SAFECODE=1

NORMAL_PROBLEM_SIZE_DIRS := \
  MultiSource/Benchmarks/Olden \
  External/SPEC/CINT2000 \
  External/SPEC/CINT2006 \
  #MultiSource/Benchmarks/Olden/bisort \
  #External/SPEC/CINT2000/197.parser \
  #External/SPEC/CINT2000/164.gzip \
  #External/SPEC/CINT2000/256.bzip2 \
  #External/Povray \
  #MultiSource/Benchmarks/Olden/tsp \
  #SingleSource/Benchmarks \
  #MultiSource/Benchmarks/Olden/bh \
  #MultiSource/Benchmarks/Olden/em3d \
  #External/SPEC/CINT2000/181.mcf \
  #External/SPEC/CINT2000/254.gap \
  #External/SPEC/CINT2000/300.twolf \
  #External/SPEC/CINT2000/175.vpr \
  #External/SPEC/CINT2000/186.crafty \
  #MultiSource/Benchmarks/Olden/mst \
  #External/SPEC/CINT2000/197.parser \
  #MultiSource/Benchmarks/Olden/power \
  #External/SPEC/CINT2000/253.perlbmk \
  #External/SPEC/CINT2000/252.eon \
  #External/SPEC/CINT2000/186.crafty \
  #External/SPEC/CINT2000/181.mcf \
  #External/SPEC/CINT2000/164.gzip \
  #External/SPEC/CINT2000/255.vortex \
  #External/SPEC/CINT2000/175.vpr \
  #External/SPEC/CINT2000/186.crafty \
  #External/SPEC/CINT2000/254.gap \
  ## WORKING: \
  # \
  #External/SPEC/CINT2000/164.gzip \
  #External/SPEC/CINT2000/197.parser \
  ## FULL LIST: \
  # \
  #MultiSource/Benchmarks/Olden \
  #External/SPEC/CINT2000 \
  #External/SPEC/CINT2000/181.mcf \
  #MultiSource/Benchmarks/Olden/voronoi \
  #External/SPEC/CINT2000/176.gcc \
  #External/Namd \
  #External/SPEC/CINT2000/175.vpr \
  #External/SPEC/CINT2000 \
  #External/SPEC/CINT2000/252.eon \
  #MultiSource/Applications/Burg \
  #MultiSource/Benchmarks/Olden/health \
  #MultiSource/Benchmarks/Olden/perimeter \
  #MultiSource/Benchmarks/Olden/treeadd \
#  MultiSource/Benchmarks/Olden \
#  MultiSource/Benchmarks/Olden/em3d \
#   MultiSource/Benchmarks/Olden \
#   MultiSource/Benchmarks/Ptrdist \
#   SingleSource \
#   External/SPEC/CINT2000/300.twolf \
#   SingleSource \
#   MultiSource \
#   MultiSource/Benchmarks \
#   External/SPEC/CINT2000 \
   #External/SPEC/CFP2000/188.ammp \
   #MultiSource/Applications/ghttpd \
   #MultiSource/Benchmarks/Olden/bh \
   #MultiSource/Benchmarks/Ptrdist \
   #MultiSource/Applications \
   #MultiSource/Benchmarks


NORMAL_PROBLEM_SIZE_DIRS := \
   $(addprefix $(TEST_OBJDIR)/,$(NORMAL_PROBLEM_SIZE_DIRS))

##############################################################################
# Target for preparing the test suite for a clean run
##############################################################################
prepare:
	    (cd $(TEST_OBJDIR); $(MAKE) clean)
               
##############################################################################
# Targets for gathering statistics for programs for papers
##############################################################################
               
# Program tests for Parallel Checking
partest::
	for dir in $(NORMAL_PROBLEM_SIZE_DIRS); do \
	    (cd $$dir; \
               PROJECT_DIR=$(PROJ_OBJ_ROOT) $(MAKE) -j1 TEST=par \
               POOLALLOC_OBJDIR=$(POOLALLOC_OBJDIR) \
               $(LARGESIZE) $(STABLERUN) $(OPTIMIZED) \
                   report.html report.csv; \
			mkdir -p $(PROFILING_RESULT_DIR)/`basename $$dir`; \
			mv $(PROFILING_DAT_DIR)/profiler.*.dat $(PROFILING_RESULT_DIR)/`basename $$dir` || true;) \
			done
	@for dir in $(NORMAL_PROBLEM_SIZE_DIRS); do \
	    (cd $$dir; \
               PROJECT_DIR=$(PROJ_OBJ_ROOT) $(MAKE) -s -j1 TEST=par \
                   report) \
        done

# Program tests for Pool Allocation
progtest::
	for dir in $(LARGE_PROBLEM_SIZE_DIRS); do \
            (cd $$dir; \
               PROJECT_DIR=$(PROJ_OBJ_ROOT) $(MAKE) TEST=safecode \
               POOLALLOC_OBJDIR=$(POOLALLOC_OBJDIR) \
               RUNTIMELIMIT=$(RUNTIMELIMIT) \
               LARGE_PROBLEM_SIZE=1 \
               report.html report.csv) \
        done
	for dir in $(NORMAL_PROBLEM_SIZE_DIRS); do \
	    (cd $$dir; \
               PROJECT_DIR=$(PROJ_OBJ_ROOT) $(MAKE) TEST=safecode \
               POOLALLOC_OBJDIR=$(POOLALLOC_OBJDIR) \
               RUNTIMELIMIT=$(RUNTIMELIMIT) \
               $(LARGESIZE) $(STABLERUN) $(OPTIMIZED) \
                   report.html report.csv; \
mkdir -p $(PROFILING_RESULT_DIR)/`basename $$dir`; \
mv $(PROFILING_DAT_DIR)/profiler.*.dat $(PROFILING_RESULT_DIR)/`basename $$dir` || true;) \
        done
	@for dir in $(LARGE_PROBLEM_SIZE_DIRS); do \
            (cd $$dir;  \
               PROJECT_DIR=$(PROJ_OBJ_ROOT) $(MAKE) -s TEST=safecode \
                   LARGE_PROBLEM_SIZE=1 report) \
        done
	@for dir in $(NORMAL_PROBLEM_SIZE_DIRS); do \
	    (cd $$dir; \
               PROJECT_DIR=$(PROJ_OBJ_ROOT) $(MAKE) -s TEST=safecode \
                   report) \
        done
	@printf "\a"; sleep 1; printf "\a"; sleep 1; printf "\a"

# Program tests for SAFECode debug tool
progdebug::
	for dir in $(LARGE_PROBLEM_SIZE_DIRS); do \
            (cd $$dir; \
               USE_LTO=$(USE_LTO) \
               PROJECT_DIR=$(PROJ_OBJ_ROOT) $(MAKE) TEST=debug \
               POOLALLOC_OBJDIR=$(POOLALLOC_OBJDIR) \
               RUNTIMELIMIT=$(RUNTIMELIMIT) \
               LARGE_PROBLEM_SIZE=1 \
               report.html report.csv) \
        done
	for dir in $(NORMAL_PROBLEM_SIZE_DIRS); do \
	    (cd $$dir; \
               USE_LTO=$(USE_LTO) \
               PROJECT_DIR=$(PROJ_OBJ_ROOT) $(MAKE) TEST=debug \
               POOLALLOC_OBJDIR=$(POOLALLOC_OBJDIR) \
               RUNTIMELIMIT=$(RUNTIMELIMIT) $(DEBUG_SAFECODE) \
               $(LARGESIZE) $(STABLERUN) $(OPTIMIZED) \
                   report.html report.csv; \
      ) \
        done
	@for dir in $(LARGE_PROBLEM_SIZE_DIRS); do \
            (cd $$dir;  \
               PROJECT_DIR=$(PROJ_OBJ_ROOT) $(MAKE) -s TEST=debug \
                   LARGE_PROBLEM_SIZE=1 report) \
        done
	@for dir in $(NORMAL_PROBLEM_SIZE_DIRS); do \
	    (cd $$dir; \
               PROJECT_DIR=$(PROJ_OBJ_ROOT) $(MAKE) -s TEST=debug \
                   report) \
        done
	@printf "\a"; sleep 1; printf "\a"; sleep 1; printf "\a"

progbb::
	for dir in $(LARGE_PROBLEM_SIZE_DIRS); do \
            (cd $$dir; \
               PROJECT_DIR=$(PROJ_OBJ_ROOT) $(MAKE) TEST=bb \
               POOLALLOC_OBJDIR=$(POOLALLOC_OBJDIR) \
               RUNTIMELIMIT=$(RUNTIMELIMIT) \
               LARGE_PROBLEM_SIZE=1 \
               report.html report.csv) \
        done
	for dir in $(NORMAL_PROBLEM_SIZE_DIRS); do \
	    (cd $$dir; \
               PROJECT_DIR=$(PROJ_OBJ_ROOT) $(MAKE) TEST=bb \
               POOLALLOC_OBJDIR=$(POOLALLOC_OBJDIR) \
               RUNTIMELIMIT=$(RUNTIMELIMIT) $(DEBUG_SAFECODE) \
               $(LARGESIZE) $(STABLERUN) $(OPTIMIZED) \
                   report.html report.csv; \
      ) \
        done
	@for dir in $(LARGE_PROBLEM_SIZE_DIRS); do \
            (cd $$dir;  \
               PROJECT_DIR=$(PROJ_OBJ_ROOT) $(MAKE) -s TEST=bb \
                   LARGE_PROBLEM_SIZE=1 report) \
        done
	@for dir in $(NORMAL_PROBLEM_SIZE_DIRS); do \
	    (cd $$dir; \
               PROJECT_DIR=$(PROJ_OBJ_ROOT) $(MAKE) -s TEST=bb \
                   report) \
        done
	@printf "\a"; sleep 1; printf "\a"; sleep 1; printf "\a"

# Program tests for measuring all the different SAFECode overhead
progfull::
	for dir in $(LARGE_PROBLEM_SIZE_DIRS); do \
            (cd $$dir; \
               PROJECT_DIR=$(PROJ_OBJ_ROOT) $(MAKE) TEST=full \
               POOLALLOC_OBJDIR=$(POOLALLOC_OBJDIR) \
               RUNTIMELIMIT=$(RUNTIMELIMIT) \
               LARGE_PROBLEM_SIZE=1 \
               report.html report.csv) \
        done
	for dir in $(NORMAL_PROBLEM_SIZE_DIRS); do \
	    (cd $$dir; \
               PROJECT_DIR=$(PROJ_OBJ_ROOT) $(MAKE) TEST=full \
               POOLALLOC_OBJDIR=$(POOLALLOC_OBJDIR) \
               RUNTIMELIMIT=$(RUNTIMELIMIT) \
               $(LARGESIZE) $(STABLERUN) $(OPTIMIZED) \
                   report.html report.csv;) \
        done
	@for dir in $(LARGE_PROBLEM_SIZE_DIRS); do \
            (cd $$dir;  \
               PROJECT_DIR=$(PROJ_OBJ_ROOT) $(MAKE) -s TEST=full \
                   LARGE_PROBLEM_SIZE=1 report) \
        done
	@for dir in $(NORMAL_PROBLEM_SIZE_DIRS); do \
	    (cd $$dir; \
               PROJECT_DIR=$(PROJ_OBJ_ROOT) $(MAKE) -s TEST=full \
                   report) \
        done
	@printf "\a"; sleep 1; printf "\a"; sleep 1; printf "\a"

progabc::
	for dir in $(LARGE_PROBLEM_SIZE_DIRS); do \
            (cd $$dir; \
               PROJECT_DIR=$(PROJ_OBJ_ROOT) $(MAKE) TEST=abc \
               POOLALLOC_OBJDIR=$(POOLALLOC_OBJDIR) LARGE_PROBLEM_SIZE=1 report.html) \
        done
	for dir in $(NORMAL_PROBLEM_SIZE_DIRS); do \
	    (cd $$dir; \
               PROJECT_DIR=$(PROJ_OBJ_ROOT) $(MAKE) TEST=abc \
                   report.html) \
        done
	@for dir in $(LARGE_PROBLEM_SIZE_DIRS); do \
            (cd $$dir; \
               PROJECT_DIR=$(PROJ_OBJ_ROOT) $(MAKE) -s TEST=abc \
                   LARGE_PROBLEM_SIZE=1 report) \
        done
	@for dir in $(NORMAL_PROBLEM_SIZE_DIRS); do \
	    (cd $$dir; \
               PROJECT_DIR=$(PROJ_OBJ_ROOT) $(MAKE) -s TEST=abc \
                   report) \
        done
	@printf "\a"; sleep 1; printf "\a"; sleep 1; printf "\a"

# Run tests comparing SAFECode and Valgrind
progvg::
	for dir in $(LARGE_PROBLEM_SIZE_DIRS); do \
            (cd $$dir; \
               PROJECT_DIR=$(PROJ_OBJ_ROOT) $(MAKE) TEST=valgrind \
               POOLALLOC_OBJDIR=$(POOLALLOC_OBJDIR) \
               RUNTIMELIMIT=$(RUNTIMELIMIT) \
               LARGE_PROBLEM_SIZE=1\
               report.html report.csv) \
        done
	for dir in $(NORMAL_PROBLEM_SIZE_DIRS); do \
	    (cd $$dir; \
               PROJECT_DIR=$(PROJ_OBJ_ROOT) $(MAKE) TEST=valgrind \
               RUNTIMELIMIT=$(RUNTIMELIMIT) \
                   report.html report.csv) \
        done
	@for dir in $(LARGE_PROBLEM_SIZE_DIRS); do \
            (cd $$dir; \
               PROJECT_DIR=$(PROJ_OBJ_ROOT) $(MAKE) -s TEST=valgrind \
                   LARGE_PROBLEM_SIZE=1 report) \
        done
	@for dir in $(NORMAL_PROBLEM_SIZE_DIRS); do \
	    (cd $$dir; \
               PROJECT_DIR=$(PROJ_OBJ_ROOT) $(MAKE) -s TEST=valgrind \
                   report) \
        done
	@printf "\a"; sleep 1; printf "\a"; sleep 1; printf "\a"

progperf::
	for dir in $(LARGE_PROBLEM_SIZE_DIRS); do \
            (cd $$dir; \
               PROJECT_DIR=$(PROJ_OBJ_ROOT) $(MAKE) -j1 TEST=perf \
                   LARGE_PROBLEM_SIZE=1 report.html report.csv) \
        done
	for dir in $(NORMAL_PROBLEM_SIZE_DIRS); do \
	    (cd $$dir; \
               PROJECT_DIR=$(PROJ_OBJ_ROOT) $(MAKE) -j1 TEST=perf \
                   report.html report.csv) \
        done
	@for dir in $(LARGE_PROBLEM_SIZE_DIRS); do \
            (cd $$dir; \
               PROJECT_DIR=$(PROJ_OBJ_ROOT) $(MAKE) -s -j1 TEST=perf \
                   LARGE_PROBLEM_SIZE=1 report report.csv) \
        done
	@for dir in $(NORMAL_PROBLEM_SIZE_DIRS); do \
	    (cd $$dir; \
               PROJECT_DIR=$(PROJ_OBJ_ROOT) $(MAKE) -s -j1 TEST=perf \
                   report report.csv) \
        done
	@printf "\a"; sleep 1; printf "\a"; sleep 1; printf "\a"

progp4perf::
	for dir in $(LARGE_PROBLEM_SIZE_DIRS); do \
            (cd $$dir; \
               PROJECT_DIR=$(PROJ_OBJ_ROOT) $(MAKE) -j1 TEST=p4perf \
                   LARGE_PROBLEM_SIZE=1 report.html report.csv) \
        done
	for dir in $(NORMAL_PROBLEM_SIZE_DIRS); do \
	    (cd $$dir; \
               PROJECT_DIR=$(PROJ_OBJ_ROOT) $(MAKE) -j1 TEST=p4perf \
                   report.html report.csv) \
        done
	@for dir in $(LARGE_PROBLEM_SIZE_DIRS); do \
            (cd $$dir; \
               PROJECT_DIR=$(PROJ_OBJ_ROOT) $(MAKE) -s -j1 TEST=p4perf \
                   LARGE_PROBLEM_SIZE=1 report report.csv) \
        done
	@for dir in $(NORMAL_PROBLEM_SIZE_DIRS); do \
	    (cd $$dir; \
               PROJECT_DIR=$(PROJ_OBJ_ROOT) $(MAKE) -s -j1 TEST=p4perf \
                   report report.csv) \
        done
	@printf "\a"; sleep 1; printf "\a"; sleep 1; printf "\a"

progcputrack::
	for dir in $(LARGE_PROBLEM_SIZE_DIRS); do \
            (cd $$dir; \
               PROJECT_DIR=$(PROJ_OBJ_ROOT) $(MAKE) -j1 TEST=cputrack \
                   LARGE_PROBLEM_SIZE=1 report.html report.csv) \
        done
	for dir in $(NORMAL_PROBLEM_SIZE_DIRS); do \
	    (cd $$dir; \
               PROJECT_DIR=$(PROJ_OBJ_ROOT) $(MAKE) -j1 TEST=cputrack \
                   report.html report.csv) \
        done
	@for dir in $(LARGE_PROBLEM_SIZE_DIRS); do \
            (cd $$dir; \
               PROJECT_DIR=$(PROJ_OBJ_ROOT) $(MAKE) -s -j1 TEST=cputrack \
                   LARGE_PROBLEM_SIZE=1 report report.csv) \
        done
	@for dir in $(NORMAL_PROBLEM_SIZE_DIRS); do \
	    (cd $$dir; \
               PROJECT_DIR=$(PROJ_OBJ_ROOT) $(MAKE) -s -j1 TEST=cputrack \
                   report report.csv) \
        done
	@printf "\a"; sleep 1; printf "\a"; sleep 1; printf "\a"


##############################################################################
# Targets for running tests and gathering statistics for arbitrary tests
##############################################################################

# test target - Descend into projects/test-suite and run the TEST.safecode.Makefile
# tests...
test::
	(cd $(TEST_OBJDIR)/$(SUBDIR); \
               PROJECT_DIR=$(PROJ_OBJ_ROOT) $(MAKE) -j1 TEST=safecode \
                   $(LARGESIZE) $(STABLERUN) report report.html)
	@printf "\a"; sleep 1; printf "\a"; sleep 1; printf "\a"

pacompiletime::
	(cd $(TEST_OBJDIR)/$(SUBDIR); \
               PROJECT_DIR=$(PROJ_OBJ_ROOT) $(MAKE) -j1 TEST=pacompiletime \
                   report report.html)
	@printf "\a"; sleep 1; printf "\a"; sleep 1; printf "\a"

ptrcomp::
	(cd $(TEST_OBJDIR)/$(SUBDIR); \
               PROJECT_DIR=$(PROJ_OBJ_ROOT) $(MAKE) -j1 TEST=ptrcomp \
                   $(LARGESIZE) $(STABLERUN) report report.html)
	@printf "\a"; sleep 1; printf "\a"; sleep 1; printf "\a"

ptrcomptest::
	(cd $(TEST_OBJDIR)/$(SUBDIR); \
               PROJECT_DIR=$(PROJ_OBJ_ROOT) $(MAKE) -j1 TEST=ptrcomp \
                   $(LARGESIZE) report)
	@printf "\a"; sleep 1; printf "\a"; sleep 1; printf "\a"

vtl::
	(cd $(TEST_OBJDIR)/$(SUBDIR); \
               PROJECT_DIR=$(PROJ_OBJ_ROOT) $(MAKE) -j1 TEST=pavtl \
                   test report report.csv)
	@printf "\a"; sleep 1; printf "\a"; sleep 1; printf "\a"

perf::
	(cd $(TEST_OBJDIR)/$(SUBDIR); \
               PROJECT_DIR=$(PROJ_OBJ_ROOT) $(MAKE) -j1 TEST=perf \
                   test report report.csv)
	@printf "\a"; sleep 1; printf "\a"; sleep 1; printf "\a"

optzn::
	(cd $(TEST_OBJDIR)/$(SUBDIR); \
               PROJECT_DIR=$(PROJ_OBJ_ROOT) $(MAKE) -j1 TEST=optzn \
                   $(LARGESIZE) $(STABLERUN) test report report.csv)
	@printf "\a"; sleep 1; printf "\a"; sleep 1; printf "\a"

p4perf::
	(cd $(TEST_OBJDIR)/$(SUBDIR); \
               PROJECT_DIR=$(PROJ_OBJ_ROOT) $(MAKE) -j1 TEST=p4perf \
                   test report report.csv)
	@printf "\a"; sleep 1; printf "\a"; sleep 1; printf "\a"

strace::
	(cd $(TEST_OBJDIR)/$(SUBDIR); \
               PROJECT_DIR=$(PROJ_OBJ_ROOT) $(MAKE) -j1 TEST=strace \
                   test)
	@printf "\a"; sleep 1; printf "\a"; sleep 1; printf "\a"

cputrack::
	(cd $(TEST_OBJDIR)/$(SUBDIR); \
               PROJECT_DIR=$(PROJ_OBJ_ROOT) $(MAKE) -j1 TEST=cputrack \
                   report report.csv)
	@printf "\a"; sleep 1; printf "\a"; sleep 1; printf "\a"

##===----------------------------------------------------------------------===##
# Lit tests
##===----------------------------------------------------------------------===##

.PHONY: lit litclean lit-core lit-cstdlib lit-formatstrings clean \
				lit-bodiagsuite

# Path to SAFECode libraries
SC_LIB := $(PROJ_OBJ_ROOT)/$(BuildMode)/lib
SC_BIN := $(PROJ_OBJ_ROOT)/$(BuildMode)/bin/clang

SETENV = TARGET_TRIPLE=$(TARGET_TRIPLE)           \
         PYTHON=$(PYTHON)                         \
         PATH=$(PROJ_OBJ_ROOT)/test/tools:$(PATH) \
         SC_OBJ_ROOT=$(PROJ_OBJ_ROOT)

# Path to test files
CORESRC=$(PROJ_SRC_ROOT)/test/core
CSTDLIB=$(PROJ_SRC_ROOT)/test/cstdlib
FMTSTR=$(PROJ_SRC_ROOT)/test/formatstrings
REGRSN=$(PROJ_SRC_ROOT)/test/regression
BODIAGSRC=$(PROJ_SRC_ROOT)/test/BOdiagsuite-20050808/testcases

COREOBJ=$(PROJ_OBJ_ROOT)/test/cstdlib
CSTDLIBOBJ=$(PROJ_OBJ_ROOT)/test/cstdlib
FMTSTROBJ=$(PROJ_OBJ_ROOT)/test/formatstrings
REGRSNOBJ=$(PROJ_OBJ_ROOT)/test/regression
BODIAGOBJ=$(PROJ_OBJ_ROOT)/test/bodiagsuite

TESTSCRIPT=$(PROJ_OBJ_ROOT)/test/tools/test.sh

# ulimit settings
ULIMIT="ulimit -t 30 ; ulimit -d 512000 ; ulimit -m 512000 ; ulimit -v 1500000 ;"

# Run all lit tests
lit: lit-core lit-formatstrings lit-cstdlib lit-regression lit-bodiagsuite

# Run the lit tests for core SAFECode
lit-core: $(TESTSCRIPT)
	@mkdir -p $(COREOBJ)
	$(Verb) $(SETENV) $(MAKE) -C $(LLVM_OBJ_ROOT)/test check-local-lit TESTSUITE=$(CORESRC) LIT_ARGS=-j1 ULIMIT=$(ULIMIT)

# Run the lit tests for cstdlib
lit-cstdlib: $(TESTSCRIPT)
	@mkdir -p $(CSTDLIBOBJ)
	$(Verb) $(SETENV) $(MAKE) -C $(LLVM_OBJ_ROOT)/test check-local-lit TESTSUITE=$(CSTDLIB) LIT_ARGS=-j1 ULIMIT=$(ULIMIT)

# Run the lit tests for format string functions
lit-formatstrings: $(TESTSCRIPT)
	@mkdir -p $(FMTSTROBJ)
	$(Verb) $(SETENV) $(MAKE) -C $(LLVM_OBJ_ROOT)/test check-local-lit TESTSUITE=$(FMTSTR) LIT_ARGS=-j1 ULIMIT=$(ULIMIT)

# Run regression tests
lit-regression: $(TESTSCRIPT)
	@mkdir -p $(REGRSNOBJ)
	$(Verb) $(SETENV) \
		PATH=$(PROJ_OBJ_ROOT)/$(BuildMode)/bin:$(LLVMToolDir):$(PATH) \
		$(MAKE) -C $(LLVM_OBJ_ROOT)/test check-local-lit TESTSUITE=$(REGRSN) ULIMIT=$(ULIMIT)

# All names of the files in the BOdiagsuite
BODIAG_FILE_NAMES := $(notdir $(wildcard $(BODIAGSRC)/*.c))

# List of XFAIL'd tests; handled separately
BODIAG_XFAILS := $(shell awk '!/^\#/ { print $$1 }' $(BODIAGSRC)/../xfails)

# The names of the files without errors in them
BODIAG_CORRECT_NAMES := \
	$(filter-out $(BODIAG_XFAILS),$(notdir $(wildcard $(BODIAGSRC)/*ok.c)))

# The names of the files with errors in them
BODIAG_BAD_NAMES := \
	$(filter-out $(BODIAG_XFAILS) $(BODIAG_CORRECT_NAMES),$(BODIAG_FILE_NAMES))

# The paths to the test files annotated with lit run lines.
BODIAG_ANNOTATED := $(addprefix $(BODIAGOBJ)/,$(BODIAG_FILE_NAMES))

$(BODIAG_XFAILS:%=$(BODIAGOBJ)/%): $(BODIAGOBJ)/%: $(BODIAGSRC)/%
	@mkdir -p $(BODIAGOBJ)
	@printf > $@ '/*\n * RUN: test.sh -%s -t %%t %%s\n * XFAIL: %s\n */\n' \
		`if [[ $@ == *ok.c ]]; then printf p; else printf e; fi`  \
		"`awk '!/^#/' $(BODIAGSRC)/../xfails | grep -F "$(notdir $<)" | cut -f2- -d' '`"
	@cat $< >> $@

$(BODIAG_CORRECT_NAMES:%=$(BODIAGOBJ)/%): $(BODIAGOBJ)/%: $(BODIAGSRC)/%
	@mkdir -p $(BODIAGOBJ)
	@printf '/*\n * RUN: test.sh -p -t %%t %%s\n */\n' > $@
	@cat $< >> $@

$(BODIAG_BAD_NAMES:%=$(BODIAGOBJ)/%): $(BODIAGOBJ)/%: $(BODIAGSRC)/%
	@mkdir -p $(BODIAGOBJ)
	@printf '/*\n * RUN: test.sh -e -t %%t %%s\n */\n' > $@
	@cat $< >> $@

# Build the script to run the lit tests
$(TESTSCRIPT): $(PROJ_SRC_ROOT)/test/tools/test.sh.in
	@echo Creating test script...
	@mkdir -p `dirname $@`
	@sed -e 's#@SC@#$(SC_BIN)#g' \
       -e 's#@SC_LIB@#$(SC_LIB)#g' < $< > $@
	chmod +x $@

# Run the BOdiagsuite
lit-bodiagsuite: $(TESTSCRIPT) $(BODIAG_ANNOTATED)
	@cp $(BODIAGSRC)/TestInputFile1 $(BODIAGOBJ)
	$(Verb) $(SETENV) $(MAKE) -C $(LLVM_OBJ_ROOT)/test check-local-lit \
		TESTSUITE=$(BODIAGSRC)/.. LIT_ARGS=-j2 ULIMIT=$(ULIMIT)

clean:: litclean

# Clean all files generated by the lit tests
litclean:
	-rm -f $(PROJ_OBJ_ROOT)/test/tools/test.sh
	-rm -rf $(PROJ_OBJ_ROOT)/test/core/Output
	-rm -rf $(PROJ_OBJ_ROOT)/test/cstdlib/Output
	-rm -rf $(PROJ_OBJ_ROOT)/test/formatstrings/Output
	-rm -rf $(PROJ_OBJ_ROOT)/test/regression/Output
	-rm -rf $(BODIAGOBJ)/Output
