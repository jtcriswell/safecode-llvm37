LEVEL = ../../..

PARALLEL_DIRS = \
               Support \
               ArrayBoundChecks \
               InsertPoolChecks \
               ConvertUnsafeAllocas \
               OptimizeChecks \
               RewriteOOB \
               DebugInstrumentation \
               FormatStrings \
               CStdLib \
               SoftBound \
               Utility \
               CommonMemorySafety \
               BaggyBoundsChecks \
#               StackSafety \
#               SpeculativeChecking \
#               DanglingPointers \
#               CStdLib \
#               FaultInjector \
#               IndirectCallChecks \
#               PointerChecks \

include $(LEVEL)/projects/safecode/Makefile.common

ifneq ($(OS),Darwin)
OPTIONAL_DIRS := Safecode
endif

