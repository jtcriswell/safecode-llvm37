//===- ConstraintGeneration.h - ----------------------------------------------//
// 
//                          The SAFECode Compiler 
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// Note: This code assumes that ABCPreprocess is run before.
//
//===----------------------------------------------------------------------===//

#ifndef CONSTRAINT_GENERATION
#define CONSTRAINT_GENERATION

#include "llvm/Instruction.h"
#include "llvm/Function.h"
#include "AffineExpressions.h"
#include "BottomUpCallGraph.h"

namespace llvm {

ModulePass *createConstraintGenerationPass();


namespace ABC {

struct ConstraintGeneration : public ModulePass {
  public :
    static char ID;
    ConstraintGeneration () : ModulePass ((intptr_t) &ID) {}
    const char *getPassName() const { return "Interprocedural Constraint Generation"; }
    virtual bool runOnModule(Module &M);
    std::vector<Instruction*> UnsafeGetElemPtrs;
    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
      AU.addRequired<DataLayout>();
      AU.addRequired<EQTDDataStructures>();
      AU.addRequired<BottomUpCallGraph>();
      AU.setPreservesAll();
    }
  private :
  EQTDDataStructures *cbudsPass;
  BottomUpCallGraph *buCG;
  
  typedef std::map<const Function *,FuncLocalInfo*> InfoMap;
  //This is required for getting the names/unique identifiers for variables.
  Mangler *Mang;

  //for storing local information about a function
    InfoMap fMap; 


  //Known Func Database
  std::set<string> KnownFuncDB;
    

  //for storing what control dependent blocks are already dealt with for the current
  //array access
  std::set<BasicBlock *> DoneList;

  //Initializes the KnownFuncDB
  void initialize(Module &M);
  
  //This function collects from the branch which controls the current block
  //the Successor tells the path 
  void addBranchConstraints(BranchInst *BI, BasicBlock *Successor, ABCExprTree **rootp);

  //This method adds constraints for known trusted functions
  ABCExprTree* addConstraintsForKnownFunctions(Function *kf, CallInst *CI);
    
  //Interface for getting constraints for a particular value
  void getConstraintsInternal( Value *v, ABCExprTree **rootp);

  //adds all the conditions on which the currentblock is control dependent on.
  void addControlDependentConditions(BasicBlock *currentBlock, ABCExprTree **rootp); 
  
    //Gives the return value constraints interms of its arguments 
  ABCExprTree* getReturnValueConstraints(Function *f);
  void getConstraintsAtCallSite(CallInst *CI,ABCExprTree **rootp);
  void addFormalToActual(Function *f, CallInst *CI, ABCExprTree **rootp);

  //Get the constraints on the arguments
  //This goes and looks at all call sites and ors the corresponding
  //constraints
  ABCExprTree* getArgumentConstraints(Function &F);
  
  //for simplifying the constraints 
  LinearExpr* SimplifyExpression( Value *Expr, ABCExprTree **rootp);
  
  string getValueName(const Value *V);
  void generateArrayTypeConstraintsGlobal(string var, const ArrayType *T, ABCExprTree **rootp, unsigned int numElem);
  void generateArrayTypeConstraints(string var, const ArrayType *T, ABCExprTree **rootp);

  public:
  void getConstraints( Value *v, ABCExprTree **rootp);
  
  };
}
}
#endif
