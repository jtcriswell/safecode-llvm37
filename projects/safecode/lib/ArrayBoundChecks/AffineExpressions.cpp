//===- AffineExpressions.cpp - Expression Analysis Utilities ------------ --////
//                          The SAFECode Compiler
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines a package of expression analysis utilties.
//
//===----------------------------------------------------------------------===//

#include "AffineExpressions.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/BasicBlock.h"
#include <iostream>

using namespace llvm;

//
// Method: LinearExpr (constructor)
//
// Description:
//  Initialize a new Linear Expression object.
//
// Inputs:
//  Val  - The LLVM value for which to create a linear expression.
//  Mang - A pointer to the name mangling class to use to mangle LLVM Value
//         names.
//
LinearExpr::LinearExpr (const Value *Val, OmegaMangler *Mang) {
  if (Val) {
    vList = new VarList();
    cMap = new CoefficientMap();
    vsMap = new ValStringMap();
    if (const ConstantInt *CPI = dyn_cast<ConstantInt>(Val)) {
      offSet = CPI->getSExtValue();
      exprTy = Linear;
      return;
    } else if (const ConstantInt *CPI = dyn_cast<ConstantInt>(Val)) {
      offSet = CPI->getSExtValue();
      exprTy = Linear;
      return;
    }
    offSet = 0;
    vList->push_back(Val);
    string tempstr;
    tempstr = makeNameProper(Mang->getValueName(Val));
#if 0
  } else {
    int Slot = Tab.getSlot(Val); // slot number 
    if (Slot >= 0) {
      tempstr = "l_" + itostr(Slot) + "_" +
                utostr(Val->getType()->getUniqueID()); 
    } else {
      exprTy = Unknown;
      tempstr = "unknown";
      return;
    }
  }
#endif
    (*vsMap)[Val] = tempstr;
    (*cMap)[Val] = 1;
  } else {
    exprTy =  Unknown;
  }
}

//
// Method: negate()
//
// Description:
//  Multiply a linear expression by negative one.
//
void
LinearExpr::negate() {
  mulByConstant(-1);
}

void
LinearExpr::print(ostream &out) {

  if (exprTy != Unknown) {
    VarListIt vlj = vList->begin();
    out << offSet;
    for (; vlj != vList->end(); ++vlj) 
      out << " + " << (*cMap)[*vlj] << " * " << (*vsMap)[*vlj];
  } else out << "Unknown ";
}

void
LinearExpr::printOmegaSymbols(ostream &out) {
  if (exprTy != Unknown) {
    VarListIt vlj = vList->begin();
    for (; vlj != vList->end(); ++vlj) 
      out << "symbolic  " << (*vsMap)[*vlj] << ";\n";
  } 
}

//
// Method: addLinearExpr()
//
// Description:
//  Add another linear expression to this linear expression.
//
// Inputs:
//  E - A pointer to the linear expression to add to this expression.
//
// Return value:
//  None.
//
// Notes:
//  FIXME: This code does not consider the case where this linear expression
//         is unknown.
//
void
LinearExpr::addLinearExpr(LinearExpr *E) {
  //
  // If the specified expression is not a linear expression, the sum is also
  // non-linear.
  //
  if (E->getExprType() == Unknown) {
    exprTy = Unknown;
    return;
  }

  //
  // Grab the information from the specified expression.
  //
  offSet = E->getOffSet() + offSet;
  VarList* vl = E->getVarList();
  CoefficientMap* cm = E->getCMap();
  ValStringMap* vsm = E->getVSMap();
  
  //
  // For each term in the specified expression, search for a term in this
  // expression that uses the same variable.  If a matching term is found, add
  // their coefficients.
  //
  // Otherwise, the variable for the term from the new expression does not
  // appear in this expression; just add the term at the end of the term list.
  //
  VarListIt vli = vl->begin();
  bool matched;
  for (; vli !=  vl->end(); ++vli) {
    matched = false;
    VarListIt vlj = vList->begin();
    for (; vlj != vList->end(); ++vlj) {
      if (*vli == *vlj) {
        //
        // We found a term with a matching variable.  Add the coefficients.
        //
        (*cMap)[*vli] =  (*cMap)[*vli] + (*cm)[*vlj];
        matched = true;
        break;
      }
    }

    if (!matched) {
      //
      // No term with the variable exists in this expression.
      //
      vList->push_back(*vli);
      (*cMap)[*vli] = (*cm)[*vli];
      (*vsMap)[*vli] = (*vsm)[*vli];
    }
  }
}

//
// Method: mulLinearExpr()
//
// Description:
//  Multiply this linear expression by another linear expression.
//
// Notes:
//  Currently, this method only handles multiplying a linear expression by a
//  constant.
//
LinearExpr *
LinearExpr::mulLinearExpr(LinearExpr *E) {
  //
  // If this expression or the other expression is of an unhandled form, then
  // the product of the two expressions is also unhandled (i.e., unknown).
  //
  if ((exprTy == Unknown) || (E->getExprType() == Unknown)) {
    exprTy = Unknown;
    return this;
  }

  //
  // We only support multiplying an expression by a constant.  If neither
  // expression is a constant, then make the expression unknown.
  //
  VarList* vl = E->getVarList();
  if ((vl->size() != 0) && (vList->size() != 0)) {
    exprTy = Unknown;
    return this;
  }

  //
  // Find the product of the expression and the constant.
  //
  if (vl->size() == 0) {
    // The specified expression is a constant.
    mulByConstant(E->getOffSet());
    return this;
  } else {
    // This expression is a constant.
    E->mulByConstant(offSet);
    return E;
  }
}

//
// Method: mulByConstant()
//
// Description:
//  Multiply a linear expression by a constant.
//
void
LinearExpr::mulByConstant(int E) {
  offSet = offSet * E;
  VarListIt vlj = vList->begin();
  for (; vlj != vList->end(); ++vlj) {
    (*cMap)[*vlj] = (*cMap)[*vlj] * E;
  }
}

void
Constraint::print(ostream &out) {
  out << var;
  out << rel;
  le->print(out);
}

void
Constraint::printOmegaSymbols(ostream &out) {
  if (!leConstant_) out << "symbolic " << var << ";\n";
  le->printOmegaSymbols(out);
}

void
ABCExprTree::dump() {
  print(std::cout);
}

void
ABCExprTree::print(ostream &out) {
  if (constraint != 0) {
    constraint->print(out);
  } else {
    if (logOp == "||")
      out << "((";
    left->print(out);
    if (logOp == "||")
      out << ") ";
    out << "\n" << logOp;
    if (logOp == "||")
      out << "(";
    right->print(out);
    if (logOp == "||")
      out << "))";
  }
}

void
ABCExprTree::printOmegaSymbols(ostream &out) {
  if (constraint != 0) {
    constraint->printOmegaSymbols(out);
  } else {
    left->printOmegaSymbols(out);
    right->printOmegaSymbols(out);
  }
}

