#ifndef _FIXUP_H_
#define _FIXUP_H_

#include "traversal.h"
#include "vec.h"

class Fixup : public Traversal {
 public:
  Vec<Stmt*> exprParent;
  int verify;

  Fixup::Fixup();

  virtual void preProcessStmt(Stmt* stmt);
  virtual void postProcessStmt(Stmt* stmt);
  virtual void preProcessExpr(Expr* expr);
  virtual void preProcessSymbol(Symbol* sym);
  void run(ModuleSymbol* moduleList);
};

#endif
