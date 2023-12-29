#include "ast/parse-checks.h"

#include "ast/ast.h"

#include <stdio.h>

static void check_allowed_at_file_scope_helper(
    struct Context* context,
    struct AstNode* ast,
    struct AstNode* parent) {
  if(!ast) return;

  ast_foreach(ast, a) {

    if(parent != NULL) {
      if(ast_is_type(a, ast_Function)) {
        ERROR_ON_AST(
            context,
            a,
            "function '%s' can only be declared at file scope\n",
            ast_Identifier_name(ast_Function_name(a)));
      } else if(ast_is_type(a, ast_Type) && !ast_Type_is_alias(a) && !ast_Type_is_opaque(a)) {
        ERROR_ON_AST(
            context,
            a,
            "type declarations '%s' can only be declared at file scope\n",
            ast_Identifier_name(ast_Type_name(a)));
      }
    } else {
      if(!ast_is_type(a, ast_Function) && !ast_is_type(a, ast_Type) &&
         !ast_is_type(a, ast_Variable)) {
        ERROR_ON_AST(
            context,
            a,
            "only functions, types, and variables can appear at file scope\n");
      }
    }
    ast_foreach_child(a, child) {
      check_allowed_at_file_scope_helper(context, child, a);
    }
  }
}
// functions/extern can only happen at file scope
//  type defs can only happen at file scope
static void
check_allowed_at_file_scope(struct Context* context, struct AstNode* ast) {
  check_allowed_at_file_scope_helper(context, ast, NULL);
}

static int is_loop(struct AstNode* ast) { return ast_is_type(ast, ast_While); }
static void check_break_outside_loop_helper(
    struct Context* context,
    struct AstNode* ast,
    int in_loop) {
  if(!ast) return;

  ast_foreach(ast, a) {
    if(ast_is_type(a, ast_Break) && !in_loop) {
      ERROR_ON_AST(context, a, "'break' must be used in a loop\n");
    }
    ast_foreach_child(a, c) {
      check_break_outside_loop_helper(context, c, in_loop ? 1 : is_loop(a));
    }
  }
}
// break must appear in a loop
static void
check_break_outside_loop(struct Context* context, struct AstNode* ast) {
  check_break_outside_loop_helper(context, ast, is_loop(ast));
}

static void check_return_outside_func_helper(
    struct Context* context,
    struct AstNode* ast,
    int in_func) {
  if(!ast) return;

  ast_foreach(ast, a) {
    if(ast_is_type(a, ast_Return) && !in_func) {
      ERROR_ON_AST(context, a, "'return' must be used in a function\n");
    }
    ast_foreach_child(a, c) {
      check_return_outside_func_helper(
          context,
          c,
          in_func ? 1 : ast_is_type(a, ast_Function));
    }
  }
}
// return must appear in a function
static void
check_return_outside_func(struct Context* context, struct AstNode* ast) {
  check_return_outside_func_helper(
      context,
      ast,
      ast_is_type(ast, ast_Function));
}

void hello();

void parse_checks(struct Context* context) {
  hello();
  check_break_outside_loop(context, context->ast);
  check_return_outside_func(context, context->ast);

  // this goes last, its more of as temporary check as a simplifying assumption
  check_allowed_at_file_scope(context, ast_Block_stmts(context->ast));
}
