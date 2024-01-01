#include "ast/ast.h"
#include "ast/scope-resolve.h"
#include "common/bsstring.h"
#include "common/ll-common.h"
#include "context/context.h"

void print_indent(int indent) {
  while(indent--)
    wprintf(L" ");
}

void print_scope(char* name, struct ScopeResult* sr, int indent) {
  print_indent(indent);
  wprintf(L"scope for %s\n", name);
  LL_FOREACH(sr->symbols, sym) {
    print_indent(indent + 2);
    wprintf(L"sym '%s'\n", ScopeSymbol_name(sym));
  }
}

// void do_scope_internal(struct Context* c, struct AstNode* ast, int indent) {

//   if(ast_is_type(ast, ast_Function)) {
//     struct ScopeResult* sr = scope_resolve(c, ast);
//     char* name = ast_Identifier_name(ast_Function_name(ast));
//     print_scope(name, sr, indent);
//     if(ast_Function_has_body(ast)) {
//       struct AstNode* body = ast_Function_body(ast);
//       if(!ast_Block_is_empty(body)) {
//         ast_foreach(ast_Block_stmts(body), a) {
//           do_scope_internal(c, a, indent + 4);
//         }
//       }
//     }
//   } else if(ast_is_type(ast, ast_Block)) {
//     struct ScopeResult* sr = scope_resolve(c, ast);
//     char* name = "block";
//     print_scope(name, sr, indent);
//     if(!ast_Block_is_empty(ast)) {
//       ast_foreach(ast_Block_stmts(ast), a) {
//         do_scope_internal(c, a, indent + 4);
//       }
//     }
//   } else {
//     ast_foreach_child(ast, a) {
//       if(a) do_scope_internal(c, a, indent);
//     }
//   }
// }

void do_scope_internal(struct Context* ctx, struct AstNode* ast, int indent) {
  if(ast_is_type(ast, ast_Block)) {
    struct ScopeResult* scope = scope_lookup(ctx, ast);
    print_scope("block", scope, indent);
    ast_foreach(ast_Block_stmts(ast), s) {
      do_scope_internal(ctx, s, indent + 4);
    }
  } else if(ast_is_type(ast, ast_Function) && ast_Function_has_body(ast)) {
    struct AstNode* body = ast_Function_body(ast);
    struct ScopeResult* scope = scope_lookup(ctx, body);
    char* name = ast_Identifier_name(ast_Function_name(ast));
    print_scope(bsstrcat("func: ", name), scope, indent);
    ast_foreach(ast_Block_stmts(body), s) {
      do_scope_internal(ctx, s, indent + 4);
    }
  } else {
    ast_foreach_child(ast, a) {
      if(a) do_scope_internal(ctx, a, indent);
    }
  }
}

void do_scope(struct Context* ctx) { do_scope_internal(ctx, ctx->ast, 0); }
