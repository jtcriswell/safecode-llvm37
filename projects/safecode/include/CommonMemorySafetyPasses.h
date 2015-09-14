//===- CommonMemorySafetyPasses.h - Declare common memory safety passes ---===//
// 
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This file declares the interfaces required to use the common memory
// safety passes while they are not part of LLVM.
//
//===----------------------------------------------------------------------===//

#ifndef COMMON_MEMORY_SAFETY_PASSES_H_
#define COMMON_MEMORY_SAFETY_PASSES_H_

namespace llvm {

class FunctionPass;
class ImmutablePass;
class ModulePass;
class PassRegistry;

// Insert memory access instrumentation.
FunctionPass *createInstrumentMemoryAccessesPass();
void initializeInstrumentMemoryAccessesPass(llvm::PassRegistry&);

// Memory safety check info
void initializeMSCInfoAnalysisGroup(PassRegistry&);
ImmutablePass *createCommonMSCInfoPass();
void initializeCommonMSCInfoPass(PassRegistry&);
ImmutablePass *createNoMSCInfoPass();
void initializeNoMSCInfoPass(PassRegistry&);

// Convert generic checks to fast checks where possible.
ModulePass *createExactCheckOptPass();
void initializeExactCheckOptPass(PassRegistry&);

// Remove identical load/store checks in basic blocks where possible.
FunctionPass *createOptimizeIdenticalLSChecksPass();
void initializeOptimizeIdenticalLSChecksPass(PassRegistry&);

// Remove implied fast load/store checks where possible.
FunctionPass *createOptimizeImpliedFastLSChecksPass();
void initializeOptimizeImpliedFastLSChecksPass(PassRegistry&);

}

#endif
