
#include "ast/Type.h"
#include "ast/ast.h"
#include "ast/scope-resolve.h"

#include "cg-helpers.h"
#include "cg-inst.h"

int ast_is_constant(struct AstNode* ast) {
  return ast_is_type(ast, ast_Number) || ast_is_type(ast, ast_String);
}
int ast_is_constant_expr(struct AstNode* ast) {
  if(ast_is_type(ast, ast_Expr)) {
    // currently only allows very simple constants
    if(ast_Expr_is_plain(ast) && ast_is_constant_expr(ast_Expr_lhs(ast)))
      return 1;
    else if(
        ast_Expr_is_binop(ast) && ast_Expr_op(ast) == op_CAST &&
        ast_is_constant_expr(ast_Expr_lhs(ast)))
      return 1;
    else return 0;
  }
  return ast_is_constant(ast);
}

struct cg_value* codegen_constant_expr(
    struct Context* ctx,
    struct AstNode* ast,
    struct ScopeResult* sr) {
  ASSERT(ast_is_constant_expr(ast));
  if(ast_is_type(ast, ast_Number)) {
    struct Type* t = scope_get_Type_from_ast(ctx, sr, ast, 1);
    LLVMTypeRef cg_type = get_llvm_type(ctx, sr, t);
    LLVMValueRef val;
    if(Type_is_integer(t) || Type_is_boolean(t)) {
      val = LLVMConstInt(cg_type, ast_Number_value(ast), /*signext*/ 1);
    } else {
      ASSERT(Type_is_pointer(t));

      // for now, the only pointer constant is a constant NULL
      ASSERT_MSG(
          Type_is_void(Type_get_pointee_type(t)),
          "constants of type ptr must be void");
      val = LLVMConstNull(cg_type);
    }
    return add_temp_value(ctx, val, cg_type, t);
  } else if(ast_is_type(ast, ast_String)) {
    wchar_t* str = ast_String_value(ast);
    return get_wide_string_literal(ctx, sr, str);
  } else if(ast_is_type(ast, ast_Expr)) {
    if(ast_Expr_is_plain(ast) && ast_is_constant_expr(ast_Expr_lhs(ast))) {
      return codegen_constant_expr(ctx, ast_Expr_lhs(ast), sr);
      // TODO: better cast logic
    } else if(
        ast_Expr_is_binop(ast) && ast_Expr_op(ast) == op_CAST &&
        ast_is_constant_expr(ast_Expr_lhs(ast))) {

      struct cg_value* lhsVal =
          codegen_constant_expr(ctx, ast_Expr_lhs(ast), sr);
      struct Type* rhsType =
          scope_get_Type_from_ast(ctx, sr, ast_Expr_rhs(ast), 1);

      struct cg_value* casted =
          build_const_cast(ctx, sr, lhsVal->type, lhsVal->value, rhsType);
      if(!casted) {
        ERROR_ON_AST(
            ctx,
            ast,
            "no valid cast from '%s' to '%s'\n",
            Type_to_string(lhsVal->type),
            Type_to_string(rhsType));
      }
      return casted;

    } else {
      UNIMPLEMENTED("unknown constant expr type\n");
    }
  } else {
    UNIMPLEMENTED("unknown constant type\n");
  }
}
