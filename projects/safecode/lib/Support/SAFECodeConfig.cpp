//===- SAFECodeConfig.cpp ---------------------------------------*- C++ -*----//
// 
//                          The SAFECode Compiler 
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// Parse and record all configuration parameters required by SAFECode.
//
// TODO: Move other cl::opt to this file and parse them here as appropriate
//
//===----------------------------------------------------------------------===//

#include "safecode/SAFECodeConfig.h"

#include "llvm/Support/CommandLine.h"

using namespace llvm;

NAMESPACE_SC_BEGIN

namespace {
  cl::opt<bool>
  DPChecks("dpchecks",
           cl::init(false),
           cl::desc("Perform Dangling Pointer Checks"));

  cl::opt<bool>
  RewritePtrs("rewrite-oob",
              cl::init(false),
              cl::desc("Rewrite Out of Bound (OOB) Pointers"));
  
  cl::opt<bool>
  StopOnFirstError("terminate",
                   cl::init(false),
                   cl::desc("Terminate when an Error Ocurs"));
  
  cl::opt<bool> EnableSVA("sva",
                          cl::init(false), 
                          cl::desc("Enable SVA-Kernel specific operations"));
}

namespace {
  SAFECodeConfiguration::StaticCheckTy
    none  = SAFECodeConfiguration::ABC_CHECK_NONE,
    local = SAFECodeConfiguration::ABC_CHECK_LOCAL, 
    full  = SAFECodeConfiguration::ABC_CHECK_FULL;
  
  static cl::opt<SAFECodeConfiguration::StaticCheckTy>
  StaticChecks("static-abc", cl::init(SAFECodeConfiguration::ABC_CHECK_LOCAL),
               cl::desc("Static array bounds check analysis"),
               cl::values
               (clEnumVal(none,
                          "No static array bound checks"),
                clEnumVal(local,
                          "Local static array bound checks"),
                clEnumVal(full,
                          "Omega static array bound checks"),
                clEnumValEnd));


  SAFECodeConfiguration::PATy
  single = SAFECodeConfiguration::PA_SINGLE,
    simple = SAFECodeConfiguration::PA_SIMPLE,
    multi = SAFECodeConfiguration::PA_MULTI,
    apa = SAFECodeConfiguration::PA_APA;

  static cl::opt<SAFECodeConfiguration::PATy>
  PA("pa", cl::init(simple),
     cl::desc("The type of pool allocation used by the program"),
     cl::values(
                clEnumVal(single,  "Dummy Pool Allocation (Single DS Node)"),
                clEnumVal(simple,  "Simple Pool Allocation"),
                clEnumVal(multi,   "Context-insensitive Pool Allocation"),
                clEnumVal(apa,     "Automatic Pool Allocation"),
                clEnumValEnd));
}

SAFECodeConfiguration SCConfig;

//
// Method: dpChecks()
//
// Description:
//  Determines whether the user wants dangling pointer checks enabled.
//
// Return value:
//  true  - Dangling pointer checks are enabled.
//  false - Dangling pointer checks are disabled.
//
bool
SAFECodeConfiguration::dpChecks() {
  return DPChecks;
}

//
// Method: svaEnabled()
//
// Description:
//  Determines whether the user wants the SVA features enabled.
//
// Return value:
//  true  - SVA features are enabled.
//  false - SVA features are disabled.
//
bool
SAFECodeConfiguration::svaEnabled() {
  return EnableSVA;
}

//
// Method: terminateOnErrors()
//
// Description:
//  Determines whether the user wants the generated program to terminate on
//  the first memory error detected.
//
// Return value:
//  true  - The run-time should terminate the program when a memory safety
//          violation  occurs.
//  false - The run-time should report the memory-safety error but allow the
//          program to continue execution.
//
bool
SAFECodeConfiguration::terminateOnErrors() {
  return StopOnFirstError;
}

//
// Method: rewriteOOB()
//
// Description:
//  This method determines how strict the indexing requirements are for the
//  generated program.
//
// Return value:
//  true  - Relaxes indexing options are enabled.  The program may create
//          pointers that are out-of-bounds but should not be allowed to
//          deference them.
//  false - Follow C indexing rules: a pointer must either point within a valid
//          memory object, or it can point to one byte past the end of the
//          object as long as it is never dereferenced.
//
bool
SAFECodeConfiguration::rewriteOOB() {
  return RewritePtrs;
}

//
// Method: staticCheckType()
//
// Description:
//  This method determines what algorithms should be used for static array
//  bounds checking.
//
// Return value:
//  An enumerated value indicating which type of static array bounds checking
//  should be used.
//
SAFECodeConfiguration::StaticCheckTy
SAFECodeConfiguration::staticCheckType() {
  return StaticChecks;
}

//
// Method: getPAType()
//
// Description:
//  This method examines the command-line arguments and determines which
//  version of pool allocation should be used.
//
// Return value:
//  An enumerated value that indicates which version of Pool Allocation to use.
//
SAFECodeConfiguration::PATy
SAFECodeConfiguration::getPAType() {
  return PA;
}

//
// Method: calculateDSAType()
//
// Description:
//  This method examines the various command-line arguments and determines
//  which version of DSA is needed.
//
// Return value:
//  The type of DSA analysis that SAFECode needs to use is returned.
//
SAFECodeConfiguration::DSATy
SAFECodeConfiguration::calculateDSAType() {
  struct mapping {
    PATy pa;
    DSATy dsa;
  };

  static const struct mapping M[] = {
    {PA_SINGLE, DSA_BASIC},
    {PA_SIMPLE, DSA_EQTD},
    {PA_MULTI,  DSA_STEENS},
    {PA_APA,    DSA_EQTD},
  };

  bool found = false;
  for (unsigned i = 0; i < sizeof(M) / sizeof(struct mapping); ++i) {
    if (PA == M[i].pa) {
      return M[i].dsa;
    }
  }

  assert (found && "Inconsistent usage of Pool Allocation and DSA!");
  abort();
}

NAMESPACE_SC_END
