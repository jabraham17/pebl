
#include "cg-expr.h"

#include "ast/Type.h"
#include "ast/ast.h"
#include "ast/scope-resolve.h"

#include "cg-helpers.h"
#include "cg-inst.h"
#include "cg-operator.h"

struct cg_value*
codegen_expr(struct Context* ctx, struct AstNode* ast, struct ScopeResult* sr) {
  ASSERT(ast_is_type(ast, ast_Expr));
  if(ast_Expr_is_plain(ast)) {
    return codegen_helper(ctx, ast_Expr_lhs(ast), sr);
  } else if(ast_Expr_is_binop(ast)) {
    struct cg_value* val = codegenBinaryOperator(ctx, sr, ast);
    if(val) {
      return val;
    } else {
      ERROR_ON_AST(ctx, ast, "invalid expression '%s'\n", ast_to_string(ast));
    }
  } else {
    ASSERT(ast_Expr_is_uop(ast));
    struct cg_value* val = codegenUnaryOperator(ctx, sr, ast);
    if(val) {
      return val;
    } else {
      ERROR_ON_AST(ctx, ast, "invalid expression '%s'\n", ast_to_string(ast));
    }
  }
}
