#
# Makefile for running Buffer Overflow test suite.
# To run the tests, use make -k report.txt
#

LEVEL = ../..

include $(LEVEL)/Makefile.common

CFLAGS := -g -O0
CONFIGURATION := Debug
LLVMTOOLCURRENT := $(LLVM_OBJ_ROOT)/$(CONFIGURATION)/bin
LLVMLDPROG	:= $(LLVMTOOLCURRENT)/llvm-ld$(EXEEXT)
LLC       	:= $(LLVMTOOLCURRENT)/llc$(EXEEXT)
LOPT      	:= $(LLVMTOOLCURRENT)/opt$(EXEEXT)

DIFFPROG	:= diff

SCFLAGS := -terminate -check-every-gep-use -rewrite-oob

TEST_SRC_DIR := $(PROJ_SRC_ROOT)/test/BOdiagsuite-20050808
SCRIPT_DIR := $(PROJ_SRC_ROOT)/test/Scripts/

include $(TEST_SRC_DIR)/Makefile.rules

SRCS := $(wildcard $(TEST_SRC_DIR)/testcases/*.c)
SRCS_CORRECT := $(wildcard $(TEST_SRC_DIR)/testcases/*ok.c)
TARGET_CORRECT := $(patsubst $(TEST_SRC_DIR)/testcases/%.c,%.result-correct,$(SRCS_CORRECT))
RESULT_CORRECT := $(TEST_SRC_DIR)/Scripts/ResultForCorrectProgram.sh

SRCS_BAD := $(filter-out $(SRCS_CORRECT),$(SRCS))
TARGET_BAD := $(patsubst $(TEST_SRC_DIR)/testcases/%.c,%.result-bad,$(SRCS_BAD))
RESULT_BAD := $(TEST_SRC_DIR)/Scripts/ResultForBadProgram.sh

GEN_REPORT := $(TEST_SRC_DIR)/Scripts/GenerateReport.sh

#FIXME : Should have LLVM_SRC_ROOT
RUNTIMELIMIT := 60
RUNSAFELY := $(PROJ_SRC_ROOT)/../test-suite/RunSafely.sh $(RUNTIMELIMIT) 0

TARGET := $(TARGET_BAD) $(TARGET_CORRECT) 

.PRECIOUS: $(TARGET)
.PHONY:
report.txt:: prepare $(TARGET)
	$(GEN_REPORT) > $@

%.result-correct : %.diff
	@$(RESULT_CORRECT) $< > $@

%.result-bad : $(TEST_SRC_DIR)/testcases/%.c %.sc-out
	@$(RESULT_BAD) $(TEST_SRC_DIR)/testcases/$*.c $*.sc-out > $@

%.diff : %.nat-out %.sc-out
	@$(DIFFPROG) $*.nat-out $*.sc-out > $@ || true

%.nat-out : %.nat
	@$(RUNSAFELY) /dev/null $@ ./$<
	@rm $@.time

%.sc-out : %.sc
	@$(RUNSAFELY) /dev/null $@ ./$<
	@rm $@.time


%.nat : $(TEST_SRC_DIR)/testcases/%.c
	@$(CC) $(CFLAGS) $< -o $@

%.bc : $(TEST_SRC_DIR)/testcases/%.c
	@$(LLVMGCC) $(CFLAGS) -c -emit-llvm $< -o $@

.PHONY:
prepare:
	@cp $(TEST_SRC_DIR)/testcases/TestInputFile1 .

.PHONY:
clean::
	@rm -f report.txt *.{diff,nat,sc,sc-out,nat-out,time,bc} *.result-{correct,bad} 

