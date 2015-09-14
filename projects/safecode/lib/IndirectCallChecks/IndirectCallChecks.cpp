//===- IndirectCallChecks.cpp: Fast Indirect Call Check Instrumentation ---===//
//                          The SAFECode Compiler
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This pass instruments code with fast checks for indirect function calls.
//
//===----------------------------------------------------------------------===//

#include "safecode/Config/config.h"
#include "llvm/Pass.h"
#include "llvm/Module.h"
#include "llvm/Function.h"
#include "llvm/Instructions.h"
#include "llvm/GlobalValue.h"
#include "llvm/Support/CallSite.h"
#include "llvm/InlineAsm.h"
#include "llvm/CallingConv.h"
#include "llvm/DerivedTypes.h"
#include "llvm/Constants.h"

#include "IndirectCallChecks.h"
#include "SCUtils.h"

#include <fstream>
#include <vector>
#include <sstream>

#define ENABLE_DSA 1
#define USING_SVA 0 //0=safecode user-space, 1=safecode kernel-space
#define IC_DEBUG 0 //print additional messages such as jump table in .s file

///////////////
#define OUTPUT_ASM_FILE "pass.s"
#define JUMP_TABLE_PREFIX "__"
#define JUMP_TABLE_BEGIN JUMP_TABLE_PREFIX "jump_table_begin" << jumpTableId <<
#define JUMP_TABLE_END JUMP_TABLE_PREFIX "jump_table_end" << jumpTableId <<
#define JUMP_TABLE_COLLECTION JUMP_TABLE_PREFIX "jump_table_collection"

#if ENABLE_DSA
#include <map>
#include <memory>

#include "dsa/CallTargets.h"
#endif

#if USING_SVA
#define LLVM_VERSION 19
#else
#define LLVM_VERSION 23
#endif

using namespace llvm;

#if LLVM_VERSION >= 20
using llvm::cerr;
#else
#include <iostream>
using std::cerr;
#endif

#if LLVM_VERSION >= 23
#define CREATE_LLVM_OBJECT(T, args) T::Create args
#else
#define CREATE_LLVM_OBJECT(T, args) new T args
#endif

#if IC_DEBUG
#define IC_DMSG(msg) cerr << "[DEBUG]: " << msg << "\n";
#define IC_PRINT(obj) (obj)->print(cerr);
#else
#define IC_DMSG(msg)
#define IC_PRINT(obj)
#endif

#define IC_WARN(msg) cerr << "[WARNING]: " << msg << "\n";
#define IC_PRINTWARN(obj) cerr << "[WARNING]: "; (obj)->print(cerr);

//
// Basic LLVM Types
//
static const Type * VoidType  = 0;

namespace {

#if ENABLE_DSA
  typedef std::set<Function*> function_set_type;

  class JumpTableEntry {
  public:

    //declare indirectFunction and register it into the module
    JumpTableEntry(const Function *target, Module* module) : target(target)  {

      std::string indirectName = buildName();

      indirectFunction = CREATE_LLVM_OBJECT(Function, (
                                                       target->getFunctionType(), 
                                                       GlobalValue::ExternalLinkage,
                                                       indirectName, 
                                                       module
                                                       ));  
    }

    void writeToStream(std::ostream &out) const {
      assert(target && indirectFunction);

      const std::string &funcName = indirectFunction->getName();

      IC_DMSG("writeToStream called for " << funcName << " entry" );

      out << ".global " << funcName << "\n"
          << funcName << ":\n"
          << "jmp " << target->getName().str() << "\n"
        ;
    }

    Function *getIndirectFunction() const {
      return indirectFunction;
    }
    const Function *getTarget() const {
      return target;
    }

  private:
    Function *indirectFunction;
    const Function *target;

    std::string buildName() const {
      std::stringstream stream;

      stream <<  JUMP_TABLE_PREFIX << target->getName().str();

      return stream.str();
    }
  };

  struct JumpTable {

  private:
    typedef std::vector<JumpTableEntry> entries_t;
    Module* module;
  public:

    JumpTable(Module* m) : module(m) {}

    template <class InputIterator>
    JumpTable(InputIterator targetsBegin, InputIterator targetsEnd, int tableId) {
      jumpTableId = tableId;

      //create the entries
      InputIterator iter = targetsBegin, end = targetsEnd;
      for(; iter != end; ++iter) {
        const Function *target = *iter;
        entries.push_back(JumpTableEntry(target, module));
      }

      std::vector<const Type *> emptyFuncTyArgs;
      FunctionType *emptyFuncTy = FunctionType::get(VoidType, emptyFuncTyArgs, false); 

      lowerBound = CREATE_LLVM_OBJECT(Function, (
                                                 emptyFuncTy,
                                                 GlobalValue::ExternalLinkage,
                                                 getName(),
                                                 module
                                                 ));

      upperBound = CREATE_LLVM_OBJECT(Function, (
                                                 emptyFuncTy,
                                                 GlobalValue::ExternalLinkage,
                                                 getNameEnd(),
                                                 module
                                                 ));
    }

    //serializes the jump table
    void writeToStream(std::ostream &out) const {

      IC_DMSG("writeToStream called for " << getName() );

      out << ".text\n"
          << ".global " << lowerBound->getName().str() << "\n"
          << lowerBound->getName().str() << ":\n"
        ;

      entries_t::const_iterator iter = entries.begin(), end = entries.end();
      for(; iter != end; ++iter) {
        iter->writeToStream(out);
      }

      out << ".global " << upperBound->getName().str() << "\n"
          << upperBound->getName().str() << ":\n"
        ;
    }

    const JumpTableEntry &findEntry(Function *target) const {
      entries_t::const_iterator iter = entries.begin(), end = entries.end();

      for(; iter != end; ++iter) {
        if(iter->getTarget() == target) {
          return *iter;
        }
      }

      //in the unlikely case we dont find an entry
      return *(static_cast<JumpTableEntry*>(NULL));
    }

    Function *getLowerBound() const {
      return lowerBound;
    }

    Function *getUpperBound() const {
      return upperBound;
    }

  private:
    int jumpTableId; //need this to emit unique begin/end labels

    //the entries in this jump table
    entries_t entries;

    Function *lowerBound;
    Function *upperBound;

    std::string getName() const {
      std::stringstream stream;

      stream <<  "" JUMP_TABLE_BEGIN "";

      return stream.str();
    }

    std::string getNameEnd() const {

      std::stringstream stream;

      stream <<  "" JUMP_TABLE_END "";

      return stream.str();
    }
  };

  class JumpTableCollection {
  private:
    //typedef hash_map<function_set_type, JumpTable, hashFunctionSet> jt_hash_type;

    typedef std::vector<JumpTable*> vec_tbl_t;

    typedef std::map<const Function *, JumpTable*> map_tbl_t;
    Module*& module;

  public:
    JumpTableCollection(Module*& m) : module(m), counter(0) {}

    ~JumpTableCollection() {
      vec_tbl_t::iterator iter, end;

      for(iter = tables.begin(), end = tables.end(); iter != end; ++iter) {
        JumpTable *jt = *iter;

        delete jt;
      }
    }

    //inserts this table into the collection
    //note that if the targets set was already in a previous table
    //then we do nothing
    //
    //if the set of targets is fresh, insert into collection
    //
    //returns the jump table for these targets
    template <class InputIterator>
    JumpTable *createTable(InputIterator targetsBegin, InputIterator targetsEnd) {

      InputIterator iter = targetsBegin, end = targetsEnd;

      assert(iter != end);

      const Function *f = *iter;

      //already have a jump table for this?
      map_tbl_t::iterator map_iter = tablesByFunction.find(f);
      if(map_iter != tablesByFunction.end()) {
        return map_iter->second;
      }

      //dont have a jump table for this, lets create one
      JumpTable *jt = new JumpTable(targetsBegin, targetsEnd, counter++);

      tables.push_back(jt);

      //register all functions with the new jump table
      for(; iter != end; ++iter) {
        f = *iter;
        tablesByFunction[f] = jt;
      }

      return jt;
    }

    //tries to find the Jump Table by the function in it
    //
    //return null if this function is not in a jump table
    JumpTable *findTable(const Function *target) const {
      map_tbl_t::const_iterator iter = tablesByFunction.find(target);

      if(iter == tablesByFunction.end())
        return NULL;
      else
        return iter->second;
    }

    //serializes all the jump tables
    void writeToStream(std::ostream &out) const {
      vec_tbl_t::const_iterator iter, end;

      IC_DMSG("writeToStream called for collection");

      for(iter = tables.begin(), end = tables.end(); iter != end; ++iter) {
        const JumpTable *jt = *iter;

        jt->writeToStream(out);
      }
    }

    void createInlineAsm(Module &M) const {
      std::vector<const Type *> emptyFuncTyArgs;
      FunctionType *emptyFuncTy = FunctionType::get(VoidType, emptyFuncTyArgs, false); 

      std::stringstream stream;
      writeToStream(stream);

      InlineAsm *assembly = InlineAsm::get(
                                           emptyFuncTy, 
                                           stream.str(), 
                                           "~{dirflag},~{fpsr},~{flags}",
                                           true
                                           );

      Function *F = CREATE_LLVM_OBJECT(Function, (
                                                  emptyFuncTy,
                                                  GlobalValue::ExternalLinkage,
                                                  JUMP_TABLE_COLLECTION,
                                                  &M
                                                  ));
      BasicBlock *BB = CREATE_LLVM_OBJECT(BasicBlock, (getGlobalContext(), "entry", F));

      CallInst *callAsm = CREATE_LLVM_OBJECT(CallInst, (assembly, "", BB));
      callAsm->setCallingConv(CallingConv::C);
      callAsm->setTailCall(true);

      CREATE_LLVM_OBJECT(ReturnInst, (getGlobalContext(), BB));

    }

  private:
    int counter;

    vec_tbl_t tables;
    map_tbl_t tablesByFunction;

  };
#endif

  struct IndirectCall : public ModulePass {

    static char ID;

    std::ofstream *asmStream;
    Module *module;

    JumpTableCollection tableCollection;

#if ENABLE_DSA/*{{{*/
    typedef std::list<CallSite>::iterator CallSiteIterator;
    typedef std::vector<Function*>::iterator CalleeIterator;

    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
      AU.addRequired<CallTargetFinder>();
    }

#endif/*}}}*/

#if LLVM_VERSION >= 20
    IndirectCall() : ModulePass((intptr_t) &ID), tableCollection(module)
#else
      IndirectCall()
#endif
    {
#if IC_DEBUG
      asmStream = new std::ofstream(OUTPUT_ASM_FILE);
#endif
    }

    ~IndirectCall() {
#if IC_DEBUG
      delete asmStream;
#endif
    }

    virtual bool runOnModule(Module &m) {
      bool changed = false;
      module = &m;

      //
      // Create needed LLVM types.
      //
      VoidType  = Type::getVoidTy(getGlobalContext());

      std::vector<Function*> functions;
      {
        //get all the functions in advance
        //otherwise when we declare indirect functions we will get into infinite loop
        Module::iterator iter, end;
        for(iter = m.begin(), end = m.end(); iter != end; ++iter) {
          functions.push_back(iter);
        }
      }

#if !ENABLE_DSA
      //without DSA we just throw in all functions together
      tableCollection.createTable(functions.begin(), functions.end());
#else
      //create jump tables using DSA
      CallTargetFinder* CTF = &getAnalysis<CallTargetFinder>();
      CallSiteIterator cs_iter, cs_end = CTF->cs_end();
      for(cs_iter = CTF->cs_begin(); cs_iter != cs_end; ++cs_iter) {
        CallSite cs = *cs_iter;
                
        if(!isIndirectCall(cs))
          continue;

        //handle incomplete callsites or 0-target callsites
        if(!CTF->isComplete(cs)) {
          IC_WARN("Call site is not complete, skipping bounds checks");
#if 0
          IC_PRINTWARN(cs.getInstruction());
#endif
          continue;
        }
        else if(CTF->begin(cs) == CTF->end(cs)) {
          IC_WARN("Callsite has no targets, skipping bounds checks");
#if 0
          IC_PRINTWARN(cs.getInstruction());
#endif
          continue;
        }
        else {
          IC_DMSG("Currently inspecting callsite: ");
          IC_PRINT(cs.getInstruction());
        }

        JumpTable *jt = tableCollection.createTable(CTF->begin(cs), CTF->end(cs));
        assert(jt);

        insertBoundaryChecks(cs, jt);
      }

#endif

      //then go to use of a function and update it to a jump table entry
      std::vector<Function*>::iterator iter, end;
      for(iter = functions.begin(), end = functions.end(); iter != end; ++iter) {
        Function *f = *iter;

        JumpTable *jt = tableCollection.findTable(f);
        if(!jt) continue; //skip functions that arent ever used indirectly

        const JumpTableEntry &entry = jt->findEntry(f);
        assert(&entry);

        changed = runOnFunction(*f, entry) || changed;
      }

#if IC_DEBUG
      //write to a pass.s file
      tableCollection.writeToStream(*asmStream);
#endif
      tableCollection.createInlineAsm(m);

      return changed;
    }

    //Split up the BasicBlock of the callsite into two and insert
    //the boundary checks for the targets of the callsite
    void insertBoundaryChecks(CallSite cs, JumpTable *jt) {
      const Type *VoidPtrTy = getVoidPtrType();

      Constant *indirectFuncFail = module->getOrInsertFunction (
                                                                "bchk_ind_fail",
                                                                VoidType,
                                                                VoidPtrTy,
                                                                NULL
                                                                );

      //%x = call %target (...)
      Instruction *I = cs.getInstruction();

      BasicBlock *topBB = I->getParent();
      BasicBlock *bottomBB = topBB->splitBasicBlock(I, "do_indirect_call");

      //we have an unconditional branch to bottomBB
      //but remove it since we'll create a conditional branch later
      topBB->getTerminator()->eraseFromParent();

      //if outside of bounds call bchk_ind_fail(target)
      //then resume execution

      Value *targetPointer = cs.getCalledValue();

      /* top:
       *  ...
       *  if (target <= jumpTableBegin || target >= jumpTableEnd)
       *      goto failed_ind_check
       *  else
       *      goto bottom
       * failed_ind_check:
       *      bchk_ind_failed(%target);
       *      goto bottom
       * bottom:
       *     %x = call %target(...)
       */

      BitCastInst *castTarget = new BitCastInst(
                                                targetPointer, 
                                                VoidPtrTy, 
                                                "",
                                                topBB
                                                );

      ICmpInst *LT = new ICmpInst(*topBB,
                                  ICmpInst::ICMP_ULT,
                                  castTarget,
                                  ConstantExpr::getBitCast(jt->getLowerBound(), VoidPtrTy),
                                  ""
                                  );
      ICmpInst *GT = new ICmpInst(*topBB,
                                  ICmpInst::ICMP_UGT,
                                  castTarget,
                                  ConstantExpr::getBitCast(jt->getUpperBound(), VoidPtrTy),
                                  ""
                                  );

      BinaryOperator *OR = BinaryOperator::CreateOr(LT, GT, "", topBB);

            
      BasicBlock *failedCheckBB = CREATE_LLVM_OBJECT(BasicBlock, (getGlobalContext(),
                                                                  "failed_ind_check", 
                                                                  bottomBB->getParent(), 
                                                                  bottomBB
                                                                  ));
      CREATE_LLVM_OBJECT(CallInst, (indirectFuncFail, castTarget, "", failedCheckBB));
      CREATE_LLVM_OBJECT(BranchInst, (bottomBB, failedCheckBB));

      CREATE_LLVM_OBJECT(BranchInst, (failedCheckBB, bottomBB, OR, topBB));
    }

    /*
     * if f's address is ever taken,
     * replace that use of f with __f
     *
     * __f will be inside a jump table 
     * with value 'jmp f'
     */
    bool runOnFunction(Function &f, const JumpTableEntry &entry) {

      bool changed = false;

      cerr << "Function: " << f.getName().str() << "\n";

      Function::use_iterator iter, end;

      Function *indirect = entry.getIndirectFunction();

      //go through all uses of this function
      for(iter = f.use_begin(), end = f.use_end(); iter != end; ++iter) {
        User *user = *iter;

        //dont replace direct calls to this func with indirect calls
        unsigned low = isa<CallInst>(user) || isa<InvokeInst>(user);

        //replace all address-taken(f) with indirect address
        unsigned high = user->getNumOperands();
        for(unsigned i = low; i < high; ++i) {
          Value *value = user->getOperand(i);

          //replace f with __f
          if(value == &f) {
            user->setOperand(i, indirect);
            changed = true;
          }
        }

      }

      return changed;
    }

    //returns true if the callsite is indirect, false if its direct
    bool isIndirectCall(CallSite &cs) {
      return !cs.getCalledFunction();
    }

  }; //end of struct IndirectCall

  char IndirectCall::ID = 0;
  RegisterPass<IndirectCall> X("indirect-call", "Indirect Call Pass");
}

namespace llvm {
  ModulePass *createIndirectCallChecksPass() {
    return new IndirectCall();
  }
}
