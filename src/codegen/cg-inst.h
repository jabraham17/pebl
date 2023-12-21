#ifndef CG_INST_H_
#define CG_INST_H_

#include "codegen/codegen-llvm.h"

struct cg_value*
codegen_helper(struct Context* ctx, struct AstNode* a, struct ScopeResult* sr);

#endif
