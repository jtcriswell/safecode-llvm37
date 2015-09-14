//===- ConstraintGeneration.cpp: Interprocedural Constraint Generation ----===//
//                          The SAFECode Compiler
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Now we use the control dependence, post dominance frontier to generate
// constraints for 
//
//===----------------------------------------------------------------------===//

#include <unistd.h>
#include "dsa/DSGraph.h"
#include "utils/fdstream.h"
#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/IR/Constants.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Support/Debug.h"
#include "llvm/Analysis/CallGraph.h"
#include "ConstraintGeneration.h"
#include <fstream>
#include <iostream>
#include <stdio.h>
#include <fcntl.h>

using namespace llvm;
using namespace ABC;

char ConstraintGeneration::ID = 0;

// The following are filled from the preprocess pass since they require
// function passes.
extern IndVarMap indMap; 

#if 0
extern DominatorSet::DomSetMapType dsmt;
extern PostDominatorSet::DomSetMapType pdsmt;
extern PostDominanceFrontier::DomSetMapType pdfmt;
#endif

static bool dominates(BasicBlock *bb1, BasicBlock *bb2) {
  DominatorSet::DomSetMapType::const_iterator dsmtI = dsmt.find(bb1);
  assert((dsmtI != dsmt.end()) && " basic block not found in dominator set");
  return (dsmtI->second.count(bb2) != 0);
}

static bool postDominates(BasicBlock *bb1, BasicBlock *bb2) {
  PostDominatorSet::DomSetMapType::const_iterator pdsmtI = pdsmt.find(bb1);
  assert((pdsmtI != pdsmt.end()) &&
	                 " basic block not found in post dominator set");
  return (pdsmtI->second.count(bb2) != 0);
}

// This will tell us whether the collection of constraints
// depends on the incoming args or not
// Do we need this to be global?
static bool reqArgs = false;
// a hack for llvm's malloc instruction which concerts all ints to uints
// This is not really necessary, as it is checked in the pool allocation
// run-time library 
static bool fromMalloc = false;

// Interprocedural ConstraintGeneration pass
RegisterPass<ConstraintGeneration> cgen1("cgen","Array Bounds Checking pass");


 void ConstraintGeneration::initialize(Module &M) {
    KnownFuncDB.insert("snprintf"); //added the format string & string check
    KnownFuncDB.insert("strcpy"); //need to add the extra checks 
    KnownFuncDB.insert("memcpy"); //need to add the extra checks 
    KnownFuncDB.insert("llvm.memcpy"); //need to add the extra checks 
    KnownFuncDB.insert("strlen"); //Gives return value constraints 
    KnownFuncDB.insert("read"); // read requires checks and return value constraints
    KnownFuncDB.insert("fread"); //need to add the extra checks 

    KnownFuncDB.insert("fprintf"); //need to check if it is not format string
    KnownFuncDB.insert("printf"); //need to check if it is not format string 
    KnownFuncDB.insert("vfprintf"); //need to check if it is not format string 
    KnownFuncDB.insert("syslog"); //need to check if it is not format string 

    KnownFuncDB.insert("memset"); //need to check if we are not setting outside
    KnownFuncDB.insert("llvm.memset"); //need to check if we are not setting outside
    KnownFuncDB.insert("gets"); // need to check if the char array is greater than 80
    KnownFuncDB.insert("strchr"); //FIXME check has not been added yet 
    KnownFuncDB.insert("sprintf"); //FIXME to add extra checks
    KnownFuncDB.insert("fscanf"); //Not sure if it requires a check

    //Not sure if the following require any checks. 
    KnownFuncDB.insert("llvm.va_start");
    KnownFuncDB.insert("llvm.va_end");
    
    //The following doesnt require checks
    KnownFuncDB.insert("random");
    KnownFuncDB.insert("rand");
    KnownFuncDB.insert("clock");
    KnownFuncDB.insert("exp");
    KnownFuncDB.insert("fork");
    KnownFuncDB.insert("wait");
    KnownFuncDB.insert("fflush");
    KnownFuncDB.insert("fclose");
    KnownFuncDB.insert("alarm");
    KnownFuncDB.insert("signal");
    KnownFuncDB.insert("setuid");
    KnownFuncDB.insert("__errno_location");
    KnownFuncDB.insert("log");
    KnownFuncDB.insert("srand48");
    KnownFuncDB.insert("drand48");
    KnownFuncDB.insert("lrand48");
    KnownFuncDB.insert("times"); 
    KnownFuncDB.insert("puts");
    KnownFuncDB.insert("putchar");
    KnownFuncDB.insert("strcmp");
    KnownFuncDB.insert("strtol");
    KnownFuncDB.insert("fopen");
    KnownFuncDB.insert("fwrite");
    KnownFuncDB.insert("fgetc");
    KnownFuncDB.insert("getc");
    KnownFuncDB.insert("open");
    KnownFuncDB.insert("feof");
    KnownFuncDB.insert("fputc");
    KnownFuncDB.insert("atol");
    KnownFuncDB.insert("atoi");
    KnownFuncDB.insert("atof");
    KnownFuncDB.insert("exit");
    KnownFuncDB.insert("perror");
    KnownFuncDB.insert("sqrt");
    KnownFuncDB.insert("floor");
    KnownFuncDB.insert("pow");
    KnownFuncDB.insert("abort");
    KnownFuncDB.insert("srand");
    KnownFuncDB.insert("perror");
    KnownFuncDB.insert("__isnan");
    KnownFuncDB.insert("__main");
    KnownFuncDB.insert("ceil");
  }
  
string ConstraintGeneration::getValueName(const Value *V) {
  return Mang->getValueName(V);
}

ABCExprTree* ConstraintGeneration::getReturnValueConstraints(Function *f) {
  bool localSave = reqArgs;
  const Type* csiType = Type::getPrimitiveType(Type::Int32TyID);
  const ConstantInt * signedzero = getGlobalContext().getConstantInt(csiType,0);
  string var = "0";
  Constraint *c = new Constraint(var, new LinearExpr(signedzero, Mang),"=");
  ABCExprTree *root = new ABCExprTree(c); //dummy constraint 
  Function::iterator bI = f->begin(), bE = f->end();
  for (;bI != bE; ++bI) {
    BasicBlock *bb = bI;
    if (ReturnInst *RI = dyn_cast<ReturnInst>(bb->getTerminator()))  
      getConstraints(RI,&root);
  }
  reqArgs = localSave ; //restore to the original
  return root;
}

void ConstraintGeneration::addFormalToActual(Function *Fn, CallInst *CI, ABCExprTree **rootp) {
  LinearExpr *le1 = new LinearExpr(CI,Mang);
  Constraint *c1 = new Constraint(getValueName(Fn),le1,"=");
  *rootp = new ABCExprTree(*rootp,new ABCExprTree(c1),"&&");
  
  Function::arg_iterator formalArgCurrent = Fn->arg_begin(),
                         formalArgEnd     = Fn->arg_end();
  for (unsigned i = 1;
       formalArgCurrent != formalArgEnd;
       ++formalArgCurrent, ++i) {
    string varName = getValueName(formalArgCurrent);
    Value *OperandVal = CI->getOperand(i);
    LinearExpr *le = new LinearExpr(OperandVal,Mang);
    Constraint* c1 = new Constraint(varName,le,"=");
    ABCExprTree *temp = new ABCExprTree(c1);
    *rootp = new ABCExprTree(*rootp, temp, "&&"); //and of all arguments
  }
}

//This is an auxillary function used by getConstraints
//gets the constraints on the return value interms of its arguments
// and ands it with the existing rootp!
void
ConstraintGeneration::getConstraintsAtCallSite (CallInst *CI,
                                                ABCExprTree **rootp) {
  if (Function *pf = dyn_cast<Function>(CI->getOperand(0))) {
    if (pf->isExternal()) {
      *rootp = new ABCExprTree(*rootp,addConstraintsForKnownFunctions(pf, CI), "&&");
      addFormalToActual(pf, CI, rootp);
    } else {
      if (buCG->isInSCC(pf)) {
        std::cerr << "Ignoring return values on function in recursion\n";
        return; 
      }
      *rootp = new ABCExprTree(*rootp,getReturnValueConstraints(pf), "&&");
      addFormalToActual(pf, CI, rootp);
    }

    //
    // Now get the constraints on the actual arguemnts for the original call
    // site 
    //
    for (unsigned i =1; i < CI->getNumOperands(); ++i) 
      getConstraints(CI->getOperand(i),rootp);
  } else {
    //Indirect Calls
    ABCExprTree *temproot = 0;
    // Loop over all of the actually called functions...
    EQTDDataStructures::callee_iterator I = cbudsPass->callee_begin(CI),
                                              E = cbudsPass->callee_end(CI);
    //assert((I != E) && "Indirect Call site doesn't have targets ???? ");
    //Actually thats fine, we ignore the return value constraints ;)
    for(; I != E; ++I) {
      CallSite CS = CallSite::get(I->first);
      if ((I->second->isExternal()) ||
          (KnownFuncDB.find(I->second->getName()) != KnownFuncDB.end())) {
        ABCExprTree * temp = addConstraintsForKnownFunctions(I->second, CI);
        addFormalToActual(I->second, CI, &temp);
        if (temproot) {
          // We need to or them 
          temproot = new ABCExprTree(temproot, temp, "||");
        } else {
          temproot = temp;
        }
      } else {
        if (buCG->isInSCC(I->second)) {
          std::cerr << "Ignoring return values on function in recursion\n";
          return;
        }
        ABCExprTree * temp = getReturnValueConstraints(I->second);
        addFormalToActual(I->second, CI, &temp);
        if (temproot) {
          temproot = new ABCExprTree(temproot, temp, "||");
        } else {
          temproot = temp;
        }
      }
    }

    if (temproot) {
      *rootp = new ABCExprTree(*rootp, temproot, "&&");

      //
      // Now get the constraints on the actual arguemnts for the original call
      // site 
      //
      for (unsigned i =1; i < CI->getNumOperands(); ++i) {
        getConstraints(CI->getOperand(i),rootp);
      }
    }
  }
}

void
ConstraintGeneration::addControlDependentConditions (BasicBlock *currentBlock,
                                                     ABCExprTree **rootp) {
  PostDominanceFrontier::const_iterator it = pdfmt.find(currentBlock);
  if (it != pdfmt.end()) {
    const PostDominanceFrontier::DomSetType &S = it->second;
    if (S.size() > 0) {
      PostDominanceFrontier::DomSetType::iterator pCurrent = S.begin(),
                                                  pEnd     = S.end();

      //
      // Check if it is control dependent on only one node.
      // If it is control dependent on only one node.
      // If it not, then there must be only one that dominates this node and
      // the rest should be dominated by this node.
      // or this must dominate every other node (incase of do while)
      bool dominated = false; 
      bool rdominated = true; //to check if this dominates every other node
      for (; pCurrent != pEnd; ++pCurrent) {
        if (*pCurrent == currentBlock) {
          rdominated = rdominated & true;
          continue;
        }
        if (!dominated) {
          if (dominates(*pCurrent, currentBlock)) {
            dominated = true;
            rdominated = false;
            continue;
          }
        }
        if (dominates(currentBlock, *pCurrent)) {
          rdominated = rdominated & true;
          continue;
        } else {
#if 0
          out << "In function " << currentBlock->getParent()->getName();
          out << "for basic block " << currentBlock->getName();
          out << "Something wrong ... " <<
                 "non affine or unstructured control flow??\n";
#endif
          dominated = false;
          break;
        }
      }

      if ((dominated) || (rdominated)) {
        //
        // Now we are sure that the control dominance is proper
        // i.e. it doesn't have unstructured control flow 
        //
        PostDominanceFrontier::DomSetType::iterator pdCurrent = S.begin(),
                                                    pdEnd     = S.end();
        for (; pdCurrent != pdEnd; ++pdCurrent) {
          BasicBlock *CBB = *pdCurrent;
          if (DoneList.find(CBB) == DoneList.end()) {
            TerminatorInst *TI = CBB->getTerminator();
            if (BranchInst *BI = dyn_cast<BranchInst>(TI)) {
              for (unsigned index = 0;
                   index < BI->getNumSuccessors();
                   ++index) {
                BasicBlock * succBlock = BI->getSuccessor(index);
                if (postDominates(currentBlock, succBlock)) {
                  DoneList.insert(CBB);
                  addControlDependentConditions(CBB,rootp);
                  addBranchConstraints(BI, BI->getSuccessor(index), rootp);
                  break;
                }
              }
            }
          }
        }
      }
    }
  }
}

//
// Adds constraints for known functions 
//
ABCExprTree*
ConstraintGeneration::addConstraintsForKnownFunctions (Function *kf,
                                                       CallInst *CI) {
  const Type* csiType = Type::getPrimitiveType(Type::Int32TyID);
  const ConstantInt * signedzero = getGlobalContext().getConstantInt(csiType,0);
  string var = "0";
  Constraint *c = new Constraint(var, new LinearExpr(signedzero, Mang),"=");
  ABCExprTree *root = new ABCExprTree(c); //dummy constraint 
  ABCExprTree **rootp = &root;
  string funcName = kf->getName();
  if (funcName == "memcpy") {
    string var = getValueName(CI->getOperand(1));
    LinearExpr *l1 = new LinearExpr(CI->getOperand(2),Mang);
    Constraint *c1 = new Constraint(var,l1,">=");
    *rootp = new ABCExprTree(*rootp,new ABCExprTree(c1),"||");
    getConstraints(CI->getOperand(1), rootp);
    getConstraints(CI->getOperand(2), rootp);
  } else if (funcName == "llvm.memcpy") {
    string var = getValueName(CI->getOperand(1));
    LinearExpr *l1 = new LinearExpr(CI->getOperand(2),Mang);
    Constraint *c1 = new Constraint(var,l1,">=");
    *rootp = new ABCExprTree(*rootp,new ABCExprTree(c1),"||");
    getConstraints(CI->getOperand(1), rootp);
    getConstraints(CI->getOperand(2), rootp);
  } else if (funcName == "strlen") {
    string var = getValueName(CI);
    const Type* csiType = Type::getPrimitiveType(Type::Int32TyID);
    const ConstantInt * signedzero = getGlobalContext().getConstantInt(csiType,0);
    
    Constraint *c = new Constraint(var, new LinearExpr(signedzero, Mang),">=");
    *rootp = new ABCExprTree(*rootp,new ABCExprTree(c),"&&");
    LinearExpr *l1 = new LinearExpr(CI->getOperand(1),Mang);
    Constraint *c1 = new Constraint(var,l1,"<");
    *rootp = new ABCExprTree(*rootp,new ABCExprTree(c1),"&&");
    getConstraints(CI->getOperand(1), rootp);
  } else if (funcName == "read") {
    string var = getValueName(CI);
    LinearExpr *l1 = new LinearExpr(CI->getOperand(3),Mang);
    Constraint *c1 = new Constraint(var,l1,"<=");
    *rootp = new ABCExprTree(*rootp,new ABCExprTree(c1),"&&");
    getConstraints(CI->getOperand(3), rootp);
  } else if (funcName == "fread") {
    string var = getValueName(CI);
    LinearExpr *l1 = new LinearExpr(CI->getOperand(2),Mang);
    LinearExpr *l2 = new LinearExpr(CI->getOperand(3),Mang);
    l2->mulLinearExpr(l1);
    Constraint *c1 = new Constraint(var,l2,"<=");
    *rootp = new ABCExprTree(*rootp,new ABCExprTree(c1),"&&");
    getConstraints(CI->getOperand(3), rootp);
    getConstraints(CI->getOperand(2), rootp);
  } else {
    //      out << funcName << " is not supported yet \n";
    // Ignoring some functions is okay as long as they are not part of the
    //one of the multiple indirect calls
    assert((CI->getOperand(0) == kf) && "Need to handle this properly \n");
  }
  return root;
}

void
ConstraintGeneration::getConstraints (Value *v, ABCExprTree **rootp) {
  string tempName1 = getValueName(v);
  LinearExpr *letemp1 = new LinearExpr(v,Mang);
  Constraint* ctemp1 = new Constraint(tempName1,letemp1,"=");
  ABCExprTree* abctemp1 = new ABCExprTree(ctemp1);
  getConstraintsInternal(v,&abctemp1);
  *rootp = new ABCExprTree(*rootp, abctemp1, "&&");
}
  
//
// Get Constraints on a value v, this assumes that the Table is correctly set
// for the function that is cal ling this 
//
void ConstraintGeneration::getConstraintsInternal (Value *v,
                                                   ABCExprTree **rootp) {
  string var;
  if (Instruction *I = dyn_cast<Instruction>(v)) {
  
    Function* func = I->getParent()->getParent();
    BasicBlock * currentBlock = I->getParent();

    // Here we need to add the post dominator stuff if necessary
    addControlDependentConditions(currentBlock, rootp);

    if (!isa<ReturnInst>(I)) {
      var = getValueName(I);
    } else {
      var = getValueName(func);
    }

    if (fMap.count(func)) {
      if (fMap[func]->inLocalConstraints(I)) { //checking the cache
        if (fMap[func]->getLocalConstraint(I) != 0) {
          *rootp = new ABCExprTree(*rootp,
                                   fMap[func]->getLocalConstraint(I),
                                   "&&");
        }

        return;
      }
    } else {
      fMap[func] = new FuncLocalInfo();
    }

    fMap[func]->addLocalConstraint(I,0);
    if (isa<SwitchInst>(I)) {
      // TODO later
    } else if (ReturnInst * ri = dyn_cast<ReturnInst>(I)) {
      if (ri->getNumOperands() > 0) {
        // For getting the constraints on return values 
        LinearExpr *l1 = new LinearExpr(ri->getOperand(0),Mang);
        Constraint *c1 = new Constraint(var,l1,"=");
        *rootp = new ABCExprTree(*rootp,new ABCExprTree(c1),"&&");
        getConstraints(ri->getOperand(0), rootp);
      }
    } else if (PHINode *p = dyn_cast<PHINode>(I)) {
      // It's a normal PhiNode
      if (indMap.count(p) > 0) {
        // We know that this is the canonical induction variable
        // First get the upper bound
        Value *UBound = indMap[p];
        LinearExpr *l1 = new LinearExpr(UBound, Mang);
        Constraint *c1 = new Constraint(var, l1, "<");
        *rootp = new ABCExprTree(*rootp,new ABCExprTree(c1),"&&");

        const Type* csiType = Type::getPrimitiveType(Type::Int32TyID);
        const ConstantInt * signedzero = getGlobalContext().getConstantInt(csiType,0);
        LinearExpr *l2 = new LinearExpr(signedzero, Mang);
        Constraint *c2 = new Constraint(var, l2, ">=");
        *rootp = new ABCExprTree(*rootp,new ABCExprTree(c2),"&&");

        getConstraints(UBound, rootp);
      }
    } else if (isa<CallInst>(I)) {
      CallInst * CI = dyn_cast<CallInst>(I);
      //First we have to check if it is an RMalloc
      if (CI->getOperand(0)->getName() == "RMalloc") {
        //It is an RMalloc, we knoe it has only one argument 
        Constraint *c = new Constraint(var, SimplifyExpression(I->getOperand(1),rootp),"=");
        *rootp = new ABCExprTree(*rootp,new ABCExprTree(c),"&&");
      } else {
        if (fMap.count(func) == 0) {
          fMap[func] = new FuncLocalInfo();
        }
        // This also get constraints for arguments of CI
        getConstraintsAtCallSite(CI, rootp);
      }
    } else if (isa<AllocationInst>(I)) {
      //
      // Note that this is for the local variables which are converted into
      // allocas, mallocs, etc.; we take care of the RMallocs (CASES work) in
      // the CallInst case
      //
      AllocationInst *AI = cast<AllocationInst>(I);
      if (const ArrayType *AT = dyn_cast<ArrayType>(AI->getType()->getElementType())) {
        // Sometime allocas have some array as their allocating constant !!
        // We then have to generate constraints for all the dimensions
        const Type* csiType = Type::getPrimitiveType(Type::Int32TyID);
        const ConstantInt * signedOne = getGlobalContext().getConstantInt(csiType,1);

        Constraint *c = new Constraint(var, new LinearExpr(signedOne, Mang),"=");
        *rootp = new ABCExprTree(*rootp,new ABCExprTree(c),"&&");
        generateArrayTypeConstraints(var, AT, rootp);
      } else {
        //
        // This is the general case where the allocas/mallocs are allocated by
        // some variable
        // Ugly hack because of the llvm front end's cast of
        // argument of malloc to uint
        //
        fromMalloc = true;
        Value *sizeVal = I->getOperand(0) ;

        //	  if (CastInst *csI = dyn_cast<CastInst>(I->getOperand(0))) {
        //	    const Type *toType = csI->getType();
        //	    const Type *fromType = csI->getOperand(0)->getType();
        //	    if ((toType->isPrimitiveType()) && (toType->getPrimitiveID() == Type::Int32TyID)) {
        //	      sizeVal = csI->getOperand(0);
        //	  }
        //	  }

        Constraint *c = new Constraint (var,
                                        SimplifyExpression(sizeVal,rootp),
                                        "=");
        fromMalloc = false;
        *rootp = new ABCExprTree(*rootp,new ABCExprTree(c),"&&");
      }
    } else if (isa<GetElementPtrInst>(I)) {
      GetElementPtrInst *GEP = cast<GetElementPtrInst>(I);
      Value *PointerOperand = I->getOperand(0);
      if (const PointerType *pType = dyn_cast<PointerType>(PointerOperand->getType()) ){
        // this is for arrays inside structs 
        if (const StructType *stype = dyn_cast<StructType>(pType->getElementType())) {
          // getelementptr *key, long 0, ubyte 0, long 18
          if (GEP->getNumOperands() == 4) {
            if (const ArrayType *aType = dyn_cast<ArrayType>(stype->getContainedType(0))) {
              int elSize = aType->getNumElements();
              if (const ConstantInt *CSI = dyn_cast<ConstantInt>(I->getOperand(3))) {
                elSize = elSize - CSI->getSExtValue();
                if (elSize == 0) {
                  //
                  // FIXME: Dirty HACK: This doesn't work for more than 2
                  // arrays in a struct!!
                  //
                  if (const ArrayType *aType2 = dyn_cast<ArrayType>(stype->getContainedType(1))) {
                    elSize = aType2->getNumElements();
                  }
                }

                const Type* csiType = Type::getPrimitiveType(Type::Int32TyID);
                const ConstantInt * signedOne = getGlobalContext().getConstantInt(csiType,elSize);
                Constraint *c = new Constraint(var, new LinearExpr(signedOne, Mang),"=");
                *rootp = new ABCExprTree(*rootp,new ABCExprTree(c),"&&");
              }
            }
          }
        }
      }

      // Dunno if this is a special case or need to be generalized
      // FIXME for now it is a special case.
      if (I->getNumOperands() == 2) {
        getConstraints(PointerOperand,rootp);
        getConstraints(GEP->getOperand(1),rootp);
        LinearExpr *L1 = new LinearExpr(GEP->getOperand(1), Mang);
        LinearExpr *L2 = new LinearExpr(PointerOperand, Mang);
        L1->negate();
        L1->addLinearExpr(L2);
        Constraint *c = new Constraint(var, L1,"=");
        *rootp = new ABCExprTree(*rootp,new ABCExprTree(c),"&&");
      }

      //
      // This is added for the special case found in the embedded bench marks
      // Normally GetElementPtrInst is taken care by the getSafetyConstraints
      // But sometimes you get a pointer to an array x = &x[0]
      // z = getelementptr x 0 0
      // getlelementptr z is equivalent to getelementptr x !
      //
      if (I->getNumOperands() == 3) {
        if (const PointerType *PT = dyn_cast<PointerType>(PointerOperand->getType())) {
          if (const ArrayType *AT = dyn_cast<ArrayType>(PT->getElementType())) {
            if (const ConstantInt *CSI = dyn_cast<ConstantInt>(I->getOperand(1))) {
              if (CSI->getSExtValue() == 0) {
                if (const ConstantInt *CSI2 = dyn_cast<ConstantInt>(I->getOperand(2))) {
                  if (CSI2->getSExtValue() == 0) {
                    //Now add the constraint

                    const Type* csiType = Type::getPrimitiveType(Type::Int32TyID);
                    const ConstantInt * signedOne = getGlobalContext().getConstantInt(csiType,AT->getNumElements());
                    Constraint *c = new Constraint(var, new LinearExpr(signedOne, Mang),"=");
                    *rootp = new ABCExprTree(*rootp,new ABCExprTree(c),"&&");
                    
                  }
                }
              }
            }
          }
        }
      }
    } else {
      Constraint *c = new Constraint(var, SimplifyExpression(I,rootp),"=");
      *rootp = new ABCExprTree(*rootp,new ABCExprTree(c),"&&");
    }
    fMap[func]->addLocalConstraint(I,*rootp); //storing in the cache
  } else if (GlobalVariable *GV = dyn_cast<GlobalVariable>(v)) {
    // Its a global variable...
    // It could be an array
    var = getValueName(GV);
    if (const ArrayType *AT = dyn_cast<ArrayType>(GV->getType()->getElementType())) {
      const Type* csiType = Type::getPrimitiveType(Type::Int32TyID);
      const ConstantInt * signedOne = getGlobalContext().getConstantInt(csiType,1);

      Constraint *c = new Constraint(var, new LinearExpr(signedOne, Mang),"=");
      *rootp = new ABCExprTree(*rootp,new ABCExprTree(c),"&&");
      generateArrayTypeConstraintsGlobal(var, AT, rootp, 1);	  
    }
  }
}

void
ConstraintGeneration::generateArrayTypeConstraintsGlobal (string var,
                                                          const ArrayType *T,
                                                          ABCExprTree **rootp,
                                                          unsigned int numElem) {
  string var1 = var + "_i";
  const Type* csiType = Type::getPrimitiveType(Type::Int32TyID);
  if (const ArrayType *AT = dyn_cast<ArrayType>(T->getElementType())) {
    const ConstantInt * signedOne = getGlobalContext().getConstantInt(csiType,1);
    Constraint *c = new Constraint(var1, new LinearExpr(signedOne, Mang),"=");
    *rootp = new ABCExprTree(*rootp,new ABCExprTree(c),"&&");
    generateArrayTypeConstraintsGlobal(var1,AT, rootp, T->getNumElements() * numElem);
  } else {
    const ConstantInt * signedOne = getGlobalContext().getConstantInt(csiType,numElem * T->getNumElements());
    Constraint *c = new Constraint(var1, new LinearExpr(signedOne, Mang),"=");
    *rootp = new ABCExprTree(*rootp,new ABCExprTree(c),"&&");
  }
}


void
ConstraintGeneration::generateArrayTypeConstraints (string var,
                                                    const ArrayType *T,
                                                    ABCExprTree **rootp) {
  string var1 = var + "_i";
  const Type* csiType = Type::getPrimitiveType(Type::Int32TyID);
  const ConstantInt * signedOne = getGlobalContext().getConstantInt(csiType,T->getNumElements());
  Constraint *c = new Constraint(var1, new LinearExpr(signedOne, Mang),"=");
  *rootp = new ABCExprTree(*rootp,new ABCExprTree(c),"&&");
  if (const ArrayType *AT = dyn_cast<ArrayType>(T->getElementType())) {
    generateArrayTypeConstraints(var1,AT, rootp);
  } else if (const StructType *ST = dyn_cast<StructType>(T->getElementType())) {
    //This will only work one level of arrays and structs
    //If there are arrays inside a struct then this will
    //not help us prove the safety of the access ....
    unsigned Size = getAnalysis<DataLayout>().getTypeSize(ST);
    string var2 = var1 + "_i";
    const Type* csiType = Type::getPrimitiveType(Type::Int32TyID);
    const ConstantInt * signedOne = getGlobalContext().getConstantInt(csiType,Size);
    Constraint *c = new Constraint(var2, new LinearExpr(signedOne, Mang),"=");
    *rootp = new ABCExprTree(*rootp,new ABCExprTree(c),"&&");
  }
}

ABCExprTree *ConstraintGeneration::getArgumentConstraints(Function & F) {
  if (buCG->isInSCC(&F)) return 0; //Ignore recursion for now
  std::set<Function *> reqArgFList;
  bool localSave = reqArgs;
 //First check if it is in cache
  ABCExprTree *root = fMap[&F]->getArgumentConstraints();
  if (root) {
    return root;
  } else {
    //Its not there in cache, so we compute it
    if (buCG->FuncCallSiteMap.count(&F)) {
std::vector<CallSite> &cslist = buCG->FuncCallSiteMap[&F];
for (unsigned idx = 0, sz = cslist.size(); idx != sz; ++idx) {
  ABCExprTree *rootCallInst = 0;
  if (CallInst *CI = dyn_cast<CallInst>(cslist[idx].getInstruction())) {
    //we need to AND the constraints on the arguments
    reqArgs = false;
    Function::arg_iterator formalArgCurrent = F.arg_begin(), formalArgEnd = F.arg_end();
    for (unsigned i = 1; formalArgCurrent != formalArgEnd; ++formalArgCurrent, ++i) {
      if (i < CI->getNumOperands()) {
  string varName = getValueName(formalArgCurrent);
  Value *OperandVal = CI->getOperand(i);
  LinearExpr *le = new LinearExpr(OperandVal,Mang);
  Constraint* c1 = new Constraint(varName,le,"=");
  ABCExprTree *temp = new ABCExprTree(c1);
  if (!isa<Constant>(OperandVal)) {
    getConstraints(OperandVal,&temp);
  }
  if (!rootCallInst)  {
    rootCallInst = temp;
  } else {
    rootCallInst = new ABCExprTree(rootCallInst, temp, "&&");
  }
      }
    }
    if (reqArgs) {
      //This Call site requires args better to maintain a set
      //and get the argument constraints once for all
      //since there could be multiple call sites from the same function
      reqArgFList.insert(CI->getParent()->getParent());
    }
  }
  if (!root) root = rootCallInst;
  else {
    root = new ABCExprTree(root, rootCallInst, "||");
  }
}
std::set<Function *>::iterator sI = reqArgFList.begin(), sE= reqArgFList.end();
for (; sI != sE ; ++sI) {
  ABCExprTree * argConstraints = getArgumentConstraints(**sI);
  if (argConstraints) root = new ABCExprTree(root, argConstraints, "&&");
}
fMap[&F]->addArgumentConstraints(root);  //store it in cache
    }
  }
  reqArgs = localSave;
  return root;
}


bool ConstraintGeneration::runOnModule(Module &M) {
  cbudsPass = &getAnalysis<EQTDDataStructures>();
  buCG = &getAnalysis<BottomUpCallGraph>();
  Mang = new Mangler(M);
  
  initialize(M);
  return false;
}


void ConstraintGeneration::addBranchConstraints(BranchInst *BI,BasicBlock *Successor, ABCExprTree **rootp) {
  //this has to be a conditional branch, otherwise we wouldnt have been here
  assert((BI->isConditional()) && "abcd wrong branch constraint");
  if (CmpInst *SCI = dyn_cast<CmpInst>(BI->getCondition())) {

    //SCI now has the conditional statement
    Value *operand0 = SCI->getOperand(0);
    Value *operand1 = SCI->getOperand(1);

    getConstraints(operand0,rootp);
    getConstraints(operand1,rootp);
      

    LinearExpr *l1 = new LinearExpr(operand1,Mang);

    string var0 = getValueName(operand0);
    Constraint *ct = 0;

    switch(SCI->getPredicate()) {
    case ICmpInst::ICMP_ULE: 
    case ICmpInst::ICMP_SLE: 
      //there are 2 cases for each opcode!
      //its the true branch or the false branch
      if (BI->getSuccessor(0) == Successor) {
        //true branch 
        ct = new Constraint(var0,l1,"<=");
      } else {
        ct = new Constraint(var0,l1,">");
      }
      break;
    case ICmpInst::ICMP_UGE: 
    case ICmpInst::ICMP_SGE: 
      if (BI->getSuccessor(0) == Successor) {
        //true branch 
        ct = new Constraint(var0,l1,">=");
      } else {
        //false branch
        ct = new Constraint(var0,l1,"<");
      }
      break;
    case ICmpInst::ICMP_ULT: 
    case ICmpInst::ICMP_SLT: 
      if (BI->getSuccessor(0) == Successor) {
        //true branch 
        ct = new Constraint(var0,l1,"<");
      } else {
        //false branch
        ct = new Constraint(var0,l1,">=");
      }
      break;
    case ICmpInst::ICMP_UGT:
    case ICmpInst::ICMP_SGT:
      if (BI->getSuccessor(0) == Successor) {
        //true branch 
        ct = new Constraint(var0,l1,">");
      } else {
        //false branch
        ct = new Constraint(var0,l1,"<=");
      }
      break;
    default :
      break;
    }
    if (ct != 0) {
      ct->print(std::cerr);
      *rootp = new ABCExprTree(*rootp,new ABCExprTree(ct),"&&");
    }
  }
}



// SimplifyExpression: Simplify a Value and return it as an affine expressions
LinearExpr* ConstraintGeneration::SimplifyExpression( Value *Expr, ABCExprTree **rootp) {
  assert(Expr != 0 && "Can't classify a null expression!");
  if (Expr->getType() == Type::FloatTy || Expr->getType() == Type::DoubleTy)
    return new LinearExpr(Expr, Mang) ;   // nothing  known return variable itself

  if ((isa<BasicBlock>(Expr)) || (isa<Function>(Expr)))
    assert(1 && "Unexpected expression type to classify!");
  if ((isa<GlobalVariable>(Expr)) || (isa<Argument>(Expr))) {
    reqArgs = true; //I know using global is ugly, fix this later 
    return new LinearExpr(Expr, Mang);
  }
  if (isa<Constant>(Expr)) {
    Constant *CPV = cast<Constant>(Expr);
    if (CPV->getType()->isIntegral()) { // It's an integral constant!
      if (dyn_cast<ConstantArray>(CPV)) {
	assert(1 && "Constant Array don't know how to get the values ");
      } else if (ConstantInt *CPI = dyn_cast<ConstantInt>(Expr)) {
	return new LinearExpr(CPI, Mang);
      }
    }
    return new LinearExpr(Expr, Mang); //nothing known, just return itself
  }
  if (isa<Instruction>(Expr)) {
    Instruction *I = cast<Instruction>(Expr);

    switch (I->getOpcode()) {       // Handle each instruction type seperately
    case Instruction::Add: {
      LinearExpr* Left =  (SimplifyExpression(I->getOperand(0), rootp));
      if (Left == 0) {
	Left = new LinearExpr(I->getOperand(0), Mang);
      }
      LinearExpr* Right = (SimplifyExpression(I->getOperand(1), rootp));
      if (Right == 0) {
	Right = new LinearExpr(I->getOperand(1), Mang);
      }
      Left->addLinearExpr(Right);
      return Left;
    }
    case Instruction::Sub: {
      LinearExpr* Left =  (SimplifyExpression(I->getOperand(0), rootp));
      if (Left == 0) {
	Left = new LinearExpr(I->getOperand(0), Mang);
      }
      LinearExpr* Right = (SimplifyExpression(I->getOperand(1), rootp));
      if (Right == 0) {
	Right = new LinearExpr(I->getOperand(1), Mang);
      }
      Right->negate();
      Left->addLinearExpr(Right);
      return Left;
    }
    case Instruction::FCmp :
    case Instruction::ICmp : {
      LinearExpr* L = new LinearExpr(I->getOperand(1),Mang);
      return L;
    };
    case Instruction::Mul :
    
      LinearExpr* Left =  (SimplifyExpression(I->getOperand(0),rootp));
      if (Left == 0) {
	Left = new LinearExpr(I->getOperand(0), Mang);
      }
      LinearExpr* Right = (SimplifyExpression(I->getOperand(1),rootp));
      if (Right == 0) {
	Right = new LinearExpr(I->getOperand(1), Mang);
      }
      return Left->mulLinearExpr(Right);
    }  // end switch
    if (isa<CastInst>(I)) {
      DEBUG(std::cerr << "dealing with cast instruction ");
      //Here we have to give constraints for 
      //FIXME .. this should be for all types not just sbyte 
      const Type *fromType = I->getOperand(0)->getType();
      const Type *toType = I->getType();
      string number1;
      string number2;
      bool addC = false;
      if (toType->isPrimitiveType() && fromType->isPrimitiveType()) {
	switch(toType->getTypeID()) {
	case Type::Int32TyID :
	  switch (fromType->getTypeID()) {
	  case Type::Int8TyID :
	    number1 = "-128";
	    number2 = "127";
	    addC = true;
	    break;
	  case Type::Int8TyID :
	    number1 = "0";
	    number2 = "255";
	    addC = true;
	  default:
	    break;
	  }
	case Type::Int32TyID :
	  switch(fromType->getTypeID()) {
	  case Type::Int32TyID :
	    //in llvm front end the malloc argument is always casted to
	    //uint! so we hack it here
	    //This hack works incorrectly for
	    //some programs so moved it to malloc itself
	    //FIXME FIXME This might give incorrect results in some cases!!!!
	    addC = true;
	    break;
	  case Type::Int8TyID :
	  case Type::Int8TyID :
	    number1 = "0";
	    number2 = "255";
	    addC = true;
	    break;
	  default :
	    break;
	  }
	default:
	  break;
	}
	if (addC) {
	  string var = getValueName(I);
	  LinearExpr *l1 = new LinearExpr(I,Mang);
	  if (number1 != "") {
	    Constraint *c1 = new Constraint(number1,l1,">=",true);
	    *rootp = new ABCExprTree(*rootp,new ABCExprTree(c1),"&&");
	  }
	  if (number2 != "") {
	    Constraint *c2 = new Constraint(number2,l1,"<=",true);
	    *rootp = new ABCExprTree(*rootp,new ABCExprTree(c2),"&&");
	  }
	  Constraint *c = new Constraint(var, SimplifyExpression(I->getOperand(0),rootp),"=");
	  *rootp = new ABCExprTree(*rootp,new ABCExprTree(c),"&&");
	  return l1;
	}
      } else {
	//special case for casts to beginning of structs
	// the case is (sbyte*) (Struct with the first element arrauy)
	//If it is a cast from struct to something else * ...
	if (const PointerType *pType = dyn_cast<PointerType>(I->getType())){
	  const Type *eltype = pType->getElementType();
	  if (eltype->isPrimitiveType() && (eltype->getTypeID() == Type::Int8TyID)) {
	    if (const PointerType *opPType = dyn_cast<PointerType>(fromType)){
	      const Type *opEltype = opPType->getElementType();
	      if (const StructType *stype = dyn_cast<StructType>(opEltype)) {
		if (const ArrayType *aType = dyn_cast<ArrayType>(stype->getContainedType(0))) {
		  if (aType->getElementType()->isPrimitiveType()) {
		    int elSize = aType->getNumElements();
		    switch (aType->getElementType()->getTypeID()) {
		    case Type::Int16Ty :
		    case Type::Int16TyID :  elSize *= 2; break;
		    case Type::Int32TyID :
		    case Type::Int32TyID :  elSize *= 4; break;
		    case Type::Int64Ty :
		    case Type::Int64TyID :  elSize *= 8; break;
		    default : break;
		    }
		    string varName = getValueName(I);
		    const Type* csiType = Type::getPrimitiveType(Type::Int32TyID);
		    const ConstantInt * signedOne = getGlobalContext().getConstantInt(csiType,elSize);
		    LinearExpr *l1 = new LinearExpr(signedOne, Mang);
		    return l1;
		  }
		}
	      }
	    }
	  }
	}
      }
      return SimplifyExpression(I->getOperand(0),rootp);
    } else  {
      getConstraints(I,rootp);
      LinearExpr* ret = new LinearExpr(I,Mang);
      return ret;
    }
  }
  // Otherwise, I don't know anything about this value!
  return 0;
}


Pass *createConstraintGenerationPass() { return new ABC::ConstraintGeneration(); }
