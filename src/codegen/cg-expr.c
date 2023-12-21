
#include "cg-expr.h"

#include "ast/TypeTable.h"
#include "ast/ast.h"

#include "cg-helpers.h"
#include "cg-inst.h"
#include "cg-operator.h"

// TODO: this is duplicated from compiler-builins.c
static struct Type* getTypeFromArg(struct Context* ctx, struct AstNode* arg) {
  if(ast_is_type(arg, ast_Identifier)) {
    char* typename = ast_Identifier_name(arg);
    struct Type* type = TypeTable_get_type(ctx, typename);
    return type;
  } else if(ast_is_type(arg, ast_Expr) && ast_Expr_is_plain(arg)) {
    struct Type* type = getTypeFromArg(ctx, ast_Expr_lhs(arg));
    return type;
  }
  return NULL;
}

struct cg_value*
codegen_expr(struct Context* ctx, struct AstNode* ast, struct ScopeResult* sr) {
  ASSERT(ast_is_type(ast, ast_Expr));
  if(ast_Expr_is_plain(ast)) {
    return codegen_helper(ctx, ast_Expr_lhs(ast), sr);
  } else if(ast_Expr_is_binop(ast)) {

    // TODO: make cast look better
    if(ast_Expr_op(ast) == op_CAST) {
      struct cg_value* lhsVal = codegen_helper(ctx, ast_Expr_lhs(ast), sr);
      struct Type* rhsType = getTypeFromArg(ctx, ast_Expr_rhs(ast));
      struct Type* lhsType = lhsVal->type;
      if(TypeTable_equivalent_types(ctx, lhsType, rhsType)) {
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
            get_llvm_type(ctx, rhsType),
            1,
            "cast");
        return allocate_stack_for_temp(
            ctx,
            get_llvm_type(ctx, rhsType),
            val,
            rhsType);
      }
    }

    struct cg_value* lhsVal = codegen_helper(ctx, ast_Expr_lhs(ast), sr);
    struct cg_value* rhsVal = codegen_helper(ctx, ast_Expr_rhs(ast), sr);
    struct cg_value* val =
        codegenBinaryOperator(ctx, ast_Expr_op(ast), lhsVal, rhsVal);
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
        codegenUnaryOperator(ctx, ast_Expr_op(ast), operandVal);
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
