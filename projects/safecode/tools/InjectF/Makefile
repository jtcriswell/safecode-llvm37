#===- tools/InjectF/Makefile -------------------------------*- Makefile -*-===##
# 
#                           SAFECode Compiler Project
#
# This file was developed by the LLVM research group and is distributed under
# the University of Illinois Open Source License. See LICENSE.TXT for details.
# 
##===----------------------------------------------------------------------===##

LEVEL = ../..
TOOLNAME=injectf
REQUIRES_EH := 1

# Initialize the USEDLIBS so we can add to it

LINK_COMPONENTS := bitreader bitwriter instrumentation scalaropts ipo
USEDLIBS        := faultinject.a LLVMDataStructure.a poolalloc.a

include $(LEVEL)/Makefile.common

#
# These rules create symbolic links from the poolalloc object tree to the
# SAFECode object tree.  This allows the LLVM build machinery to handle
# library dependencies.
#
$(PROJ_OBJ_ROOT)/$(BuildMode)/lib/libpoolalloc.a: $(POOLALLOC_OBJDIR)/$(BuildMode)/lib/libpoolalloc.a
	$(VERB) ln -fs $< $@

$(PROJ_OBJ_ROOT)/$(BuildMode)/lib/libLLVMDataStructure.a: $(POOLALLOC_OBJDIR)/$(BuildMode)/lib/libLLVMDataStructure.a
	$(VERB) ln -fs $< $@

