#ifndef CG_OPERATOR_H_
#define CG_OPERATOR_H_

#include "ast/ast.h"
#include "codegen/codegen-llvm.h"

struct cg_value* codegenBinaryOperator(
    struct Context* context,
    struct ScopeResult* scope,
    struct AstNode* expr);
struct cg_value* codegenUnaryOperator(
    struct Context* context,
    struct ScopeResult* scope,
    struct AstNode* expr);

#endif
