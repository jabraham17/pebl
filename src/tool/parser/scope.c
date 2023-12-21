#include "ast/ast.h"
#include "ast/scope-resolve.h"
#include "common/ll-common.h"
#include "context/context.h"

void print_indent(int indent) {
  while(indent--)
    printf(" ");
}

void print_scope(char* name, struct ScopeResult* sr, int indent) {
  print_indent(indent);
  printf("scope for '%s'\n", name);
  LL_FOREACH(sr->variables, sym) {
    print_indent(indent + 2);
    printf("sym '%s'\n", ScopeSymbol_name(sym));
  }
}

void do_scope_internal(struct Context* c, struct AstNode* ast, int indent) {

  if(ast_is_type(ast, ast_Function)) {
    struct ScopeResult* sr = scope_resolve(c, ast);
    char* name = ast_Identifier_name(ast_Function_name(ast));
    print_scope(name, sr, indent);
    if(!ast_Function_is_extern(ast)) {
      struct AstNode* body = ast_Function_body(ast);
      if(!ast_Block_is_empty(body)) {
        ast_foreach(ast_Block_stmts(body), a) {
          do_scope_internal(c, a, indent + 4);
        }
      }
    }
  } else if(ast_is_type(ast, ast_Block)) {
    struct ScopeResult* sr = scope_resolve(c, ast);
    char* name = "block";
    print_scope(name, sr, indent);
    if(!ast_Block_is_empty(ast)) {
      ast_foreach(ast_Block_stmts(ast), a) {
        do_scope_internal(c, a, indent + 4);
      }
    }
  } else {
    ast_foreach_child(ast, a) {
      if(a) do_scope_internal(c, a, indent);
    }
  }
}

void do_scope(struct Context* c) {
  ast_foreach(c->ast, ast) { do_scope_internal(c, ast, 0); }
}
