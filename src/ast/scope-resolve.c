#include "ast/scope-resolve.h"

#include "common/bsstring.h"
#include "common/ll-common.h"
#include "context/context.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct ScopeSymbol* ScopeSymbol_init_var(char* name, struct Type* type) {
  struct ScopeSymbol* ss = malloc(sizeof(*ss));
  memset(ss, 0, sizeof(*ss));
  ss->name = bsstrdup(name);
  ss->type = type;
  ss->is_var = 1;

  return ss;
}
static struct ScopeSymbol* ScopeSymbol_init_func(
    char* name,
    struct Type* ret_type,
    int num_args,
    struct ScopeSymbol** args) {
  struct ScopeSymbol* ss = malloc(sizeof(*ss));
  memset(ss, 0, sizeof(*ss));
  ss->name = bsstrdup(name);
  ss->type = ret_type;
  ss->is_var = 0;
  ss->num_args = num_args;
  ss->args = args;

  return ss;
}

char* ScopeSymbol_name(struct ScopeSymbol* sym) { return sym->name; }

int ScopeSymbol_is_variable(struct ScopeSymbol* sym) { return sym->is_var; }
struct Type* ScopeSymbol_type(struct ScopeSymbol* sym) { return sym->type; }

int ScopeSymbol_is_function(struct ScopeSymbol* sym) { return !sym->is_var; }
struct Type* ScopeSymbol_return_type(struct ScopeSymbol* sym) {
  return sym->type;
}
int ScopeSymbol_num_args(struct ScopeSymbol* sym) { return sym->num_args; }
struct ScopeSymbol* ScopeSymbol_arg(struct ScopeSymbol* sym, int i) {
  return sym->args[i];
}
int ScopeSymbol_eq(struct ScopeSymbol* lhs, struct ScopeSymbol* rhs) {
  return lhs == rhs;
}
static struct ScopeResult*
allocate_ScopeResult(struct Context* context, struct AstNode* scope_for) {
  struct ScopeResult* sr = malloc(sizeof(*sr));
  memset(sr, 0, sizeof(*sr));
  sr->scope_for = scope_for;
  LL_APPEND(context->scope_table, sr);
  return sr;
}

struct ScopeResult*
scope_lookup(struct Context* context, struct AstNode* scope_for) {
  LL_FOREACH(context->scope_table, sr) {
    if(sr->scope_for == scope_for) return sr;
  }
  return NULL;
}

static struct ScopeSymbol* scope_lookup_identifier_in_global_scope(
    struct Context* context,
    struct AstNode* ident) {
  ASSERT(ast_is_type(ident, ast_Identifier));
  LL_FOREACH(context->scope_table, sr) {
    struct ScopeSymbol* sym = scope_lookup_identifier(context, sr, ident, 0);
    if(sym) {
      return sym;
    }
  }

  return NULL;
}

struct ScopeSymbol* scope_lookup_identifier(
    struct Context* context,
    struct ScopeResult* sr,
    struct AstNode* ident,
    int search_parent) {
  ASSERT(ast_is_type(ident, ast_Identifier));
  if(sr == NULL) {
    return scope_lookup_identifier_in_global_scope(context, ident);
  }

  LL_FOREACH(sr->variables, var) {
    if(strcmp(ScopeSymbol_name(var), ast_Identifier_name(ident)) == 0) {
      return var;
    }
  }
  if(search_parent) {
    if(sr->parent_scope)
      return scope_lookup_identifier(
          context,
          sr->parent_scope,
          ident,
          search_parent);
    else {
      return scope_lookup_identifier_in_global_scope(context, ident);
    }
  } else return NULL;
}

static struct ScopeSymbol* build_ScopeSymbol_for_variable(
    struct Context* context,
    struct ScopeResult* sr,
    struct AstNode* var) {
  ASSERT(ast_is_type(var, ast_Variable));
  // lookup the ident, searching only the local scope, if it exists already,
  // error
  struct ScopeSymbol* ss =
      scope_lookup_identifier(context, sr, ast_Variable_name(var), 0);
  if(ss) {
    ERROR_ON_AST(
        context,
        var,
        "cannot redefine variable '%s'\n",
        ScopeSymbol_name(ss));
  } else {
    // build a new scope symbol for this variable
    struct Type* type = TypeTable_get_type_ast(context, ast_Variable_type(var));
    ss =
        ScopeSymbol_init_var(ast_Identifier_name(ast_Variable_name(var)), type);
    LL_APPEND(sr->variables, ss);
  }
  return ss;
}

static void build_scope(
    struct Context* context,
    struct ScopeResult* scope,
    struct AstNode* ast);
static struct ScopeResult* scope_resolve_for_ast(
    struct Context* context,
    struct AstNode* scope_for,
    struct ScopeResult* parent) {
  // lookup scope, if we have already resolved it, no need to resolve further
  struct ScopeResult* scope = scope_lookup(context, scope_for);
  if(scope) return scope;

  scope = allocate_ScopeResult(context, scope_for);
  scope->parent_scope = parent;
  build_scope(context, scope, scope_for);
  return scope;
}

static void build_scope(
    struct Context* context,
    struct ScopeResult* scope,
    struct AstNode* ast) {

  if(ast_is_type(ast, ast_Variable)) {
    build_ScopeSymbol_for_variable(context, scope, ast);
  } else if(ast_is_type(ast, ast_Function)) {
    // lookup the ident, searching only the local scope, if it exists already,
    // error
    struct ScopeSymbol* sym =
        scope_lookup_identifier(context, scope, ast_Function_name(ast), 0);
    if(sym) {
      ERROR_ON_AST(
          context,
          ast,
          "cannot redefine function '%s'\n",
          ScopeSymbol_name(sym));
    } else {
      // build a new scope symbol for this function
      int num_args = ast_Function_num_args(ast);
      struct ScopeSymbol** args = malloc(sizeof(*args) * num_args);
      int i = 0;
      ast_foreach(ast_Function_args(ast), a) {
        args[i] = build_ScopeSymbol_for_variable(context, scope, a);
        i++;
      }
      struct Type* ret_type =
          TypeTable_get_type_ast(context, ast_Function_ret_type(ast));
      sym = ScopeSymbol_init_func(
          ast_Identifier_name(ast_Function_name(ast)),
          ret_type,
          num_args,
          args);
    }
    LL_APPEND(scope->variables, sym);

    if(ast_Function_has_body(ast)) {
      // now we need to resolve the function body
      struct AstNode* body = ast_Function_body(ast);
      if(!ast_Block_is_empty(body)) {
        ast_foreach(ast_Block_stmts(body), elm) {
          build_scope(context, scope, elm);
        }
      }
    }
  } else if(ast_is_type(ast, ast_Block)) {

    struct ScopeResult* new_scope = allocate_ScopeResult(context, ast);
    new_scope->parent_scope = scope;

    if(!ast_Block_is_empty(ast)) {
      ast_foreach(ast_Block_stmts(ast), elm) {
        build_scope(context, new_scope, elm);
      }
    }
  } else {
    ast_foreach_child(ast, a) {
      if(a) build_scope(context, scope, a);
    }
  }
}

struct ScopeResult*
scope_resolve(struct Context* context, struct AstNode* scope_for) {
  return scope_resolve_for_ast(context, scope_for, NULL);
}
