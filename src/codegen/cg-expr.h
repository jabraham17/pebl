#ifndef CG_EXPR_H_
#define CG_EXPR_H_

#include "codegen/codegen-llvm.h"

struct cg_value*
codegen_expr(struct Context* ctx, struct AstNode* ast, struct ScopeResult* sr);

#endif
