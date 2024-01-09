#ifndef CG_INST_H_
#define CG_INST_H_

#include "codegen/codegen-llvm.h"

struct cg_value*
codegen_inst(struct Context* ctx, struct AstNode* a, struct ScopeResult* sr);

struct cg_value* codegen_function(
    struct Context* ctx,
    struct AstNode* ast,
    struct ScopeResult* sr);

struct cg_value*
codegen_call(struct Context* ctx, struct AstNode* ast, struct ScopeResult* sr);

struct cg_value* codegenBuiltin(
    struct Context* ctx,
    struct ScopeResult* scope,
    struct ScopeSymbol* builtin,
    struct AstNode* call);

struct cg_value*
codegen_expr(struct Context* ctx, struct AstNode* ast, struct ScopeResult* sr);

// TODO: these should go into ast.h, not in codegen
int ast_is_constant(struct AstNode* ast);
int ast_is_constant_expr(struct AstNode* ast);

struct cg_value* codegen_constant_expr(
    struct Context* ctx,
    struct AstNode* ast,
    struct ScopeResult* sr);

struct cg_value* codegenBinaryOperator(
    struct Context* context,
    struct ScopeResult* scope,
    struct AstNode* expr);
struct cg_value* codegenUnaryOperator(
    struct Context* context,
    struct ScopeResult* scope,
    struct AstNode* expr);

#endif
