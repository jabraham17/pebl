
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

    // TODO: make cast look better
    if(ast_Expr_op(ast) == op_CAST) {
      struct cg_value* lhsVal = codegen_helper(ctx, ast_Expr_lhs(ast), sr);
      struct Type* rhsType =
          scope_get_Type_from_ast(ctx, sr, ast_Expr_rhs(ast), 1);
      struct Type* lhsType = lhsVal->type;
      if(Type_eq(lhsType, rhsType)) {
        return lhsVal;
      } else {
        LLVMValueRef val = LLVMBuildLoad2(
            ctx->codegen->builder,
            lhsVal->cg_type,
            lhsVal->value,
            "");
        val = LLVMBuildIntCast2(
            ctx->codegen->builder,
            val,
            get_llvm_type(ctx, sr, rhsType),
            1,
            "cast");
        return allocate_stack_for_temp(
            ctx,
            get_llvm_type(ctx, sr, rhsType),
            val,
            rhsType);
      }
    }

    struct cg_value* lhsVal = codegen_helper(ctx, ast_Expr_lhs(ast), sr);
    struct cg_value* rhsVal = codegen_helper(ctx, ast_Expr_rhs(ast), sr);
    struct cg_value* val =
        codegenBinaryOperator(ctx, sr, ast_Expr_op(ast), lhsVal, rhsVal);
    if(val) {
      return val;
    } else {
      struct Location* loc = Context_get_location(ctx, ast);
      int lineno = -1;
      if(loc) {
        lineno = loc->line_start;
      }
      UNIMPLEMENTED("unimplemented binary expr on line %d\n", lineno);
    }
  } else {
    ASSERT(ast_Expr_is_uop(ast));
    struct cg_value* operandVal = codegen_helper(ctx, ast_Expr_lhs(ast), sr);
    struct cg_value* val =
        codegenUnaryOperator(ctx, sr, ast_Expr_op(ast), operandVal);
    if(val) {
      return val;
    } else {
      struct Location* loc = Context_get_location(ctx, ast);
      int lineno = -1;
      if(loc) {
        lineno = loc->line_start;
      }
      UNIMPLEMENTED("unimplemented unary expr on line %d\n", lineno);
    }
  }
}
