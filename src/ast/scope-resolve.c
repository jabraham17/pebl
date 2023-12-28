#include "ast/scope-resolve.h"

#include "ast/ast.h"
#include "common/bsstring.h"
#include "common/ll-common.h"
#include "context/context.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct ScopeSymbol* ScopeSymbol_allocate(enum ScopeSymbolType sst) {
  struct ScopeSymbol* ss = malloc(sizeof(*ss));
  memset(ss, 0, sizeof(*ss));
  ss->sst = sst;
  return ss;
}

static struct ScopeVariable*
ScopeVariable_allocate(struct AstNode* variable, struct Type* type) {
  struct ScopeVariable* var = malloc(sizeof(*var));
  memset(var, 0, sizeof(*var));
  var->variable = variable;
  var->type = type;

  return var;
}

static struct ScopeSymbol*
ScopeSymbol_init_var(struct AstNode* variable, struct Type* type) {
  struct ScopeSymbol* ss = ScopeSymbol_allocate(sst_Variable);
  ss->ss_variable = ScopeVariable_allocate(variable, type);
  return ss;
}
static struct ScopeSymbol* ScopeSymbol_init_type(struct Type* type) {
  struct ScopeSymbol* ss = ScopeSymbol_allocate(sst_Type);
  ss->ss_type = type;

  return ss;
}

static struct ScopeFunction* ScopeFunction_allocate(
    struct AstNode* function,
    struct Type* rettype,
    int num_args) {
  struct ScopeFunction* func = malloc(sizeof(*func));
  memset(func, 0, sizeof(*func));
  func->function = function;
  func->rettype = rettype;
  func->num_args = num_args;
  func->args = malloc(sizeof(*func->args) * func->num_args);
  return func;
}

static struct ScopeSymbol*
ScopeSymbol_init_func(struct AstNode* function, struct Type* rettype) {
  struct ScopeSymbol* ss = ScopeSymbol_allocate(sst_Function);
  ss->ss_function = ScopeFunction_allocate(
      function,
      rettype,
      ast_Function_num_args(function));

  return ss;
}

static struct ScopeResult* allocate_ScopeResult(
    struct Context* ctx,
    struct AstNode* ast,
    struct ScopeResult* parent) {
  ASSERT(ast_is_type(ast, ast_Block));
  struct ScopeResult* sr = malloc(sizeof(*sr));
  memset(sr, 0, sizeof(*sr));
  sr->ast = ast;
  sr->parent_scope = parent;
  LL_APPEND(ctx->scope_table, sr);
  return sr;
}

static void check_for_redefinition(
    struct Context* ctx,
    struct ScopeResult* sr,
    struct AstNode* ast) {
  // lookup the ident, searching only the local scope, if it exists already,
  // error
  char* name;
  char* redef_type;
  if(ast_is_type(ast, ast_Variable)) {
    name = ast_Identifier_name(ast_Variable_name(ast));
    redef_type = "variable";
  } else if(ast_is_type(ast, ast_Type)) {
    name = ast_Identifier_name(ast_Type_name(ast));
    redef_type = "type";
  } else if(ast_is_type(ast, ast_Function)) {
    name = ast_Identifier_name(ast_Function_name(ast));
    redef_type = "function";
  } else {
    UNIMPLEMENTED("unimplemented redefintion check\n");
  }
  struct ScopeSymbol* ss = scope_lookup_name(ctx, sr, name, 0);
  if(ss) {
    ERROR_ON_AST(ctx, ast, "cannot redefine %s '%s'\n", redef_type, name);
  }
}

static struct ScopeSymbol* build_ScopeSymbol_for_variable(
    struct Context* ctx,
    struct ScopeResult* sr,
    struct AstNode* var) {
  ASSERT(ast_is_type(var, ast_Variable));

  check_for_redefinition(ctx, sr, var);

  // build a new scope symbol for this variable
  struct AstNode* typename = ast_Variable_type(var);
  struct Type* type = NULL;
  if(typename) {
    type = scope_get_Type_from_ast(ctx, sr, typename, 1);
  } else {
    struct AstNode* init_expr = ast_Variable_expr(var);
    if(init_expr) {
      type = scope_get_Type_from_ast(ctx, sr, init_expr, 1);
    } else {
      ERROR_ON_AST(
          ctx,
          var,
          "could not infer type of '%s'\n",
          ast_Identifier_name(ast_Variable_name(var)));
    }
  }
  struct ScopeSymbol* ss = ScopeSymbol_init_var(var, type);
  LL_APPEND(sr->symbols, ss);

  return ss;
}

static struct Type* Type_allocate(char* name) {
  struct Type* t = malloc(sizeof(*t));
  memset(t, 0, sizeof(*t));

  int len = strlen(name) + 1;
  t->name = malloc(sizeof(*t->name) * len);
  bsstrcpy(t->name, name);

  return t;
}

static struct TypeField* TypeField_allocate(char* name) {
  struct TypeField* t = malloc(sizeof(*t));
  memset(t, 0, sizeof(*t));

  int len = strlen(name) + 1;
  t->name = malloc(sizeof(*t->name) * len);
  bsstrcpy(t->name, name);
  return t;
}

static void Type_init_Builtin(struct Type* t, int size) {
  t->kind = tk_BUILTIN;
  t->size = size;
}

static void Type_init_Alias(struct Type* t, struct Type* alias_of) {
  t->kind = tk_ALIAS;
  t->alias_of = alias_of;
  t->size = alias_of->size;
}

static void Type_init_Opaque(struct Type* t) {
  t->kind = tk_OPAQUE;
  t->size = -1;
}

// always allocates, want something that looks up the existing type
static void Type_init_Typedef(
    struct Context* ctx,
    struct ScopeResult* sr,
    struct Type* t,
    struct AstNode* args) {
  t->kind = tk_TYPEDEF;
  int size = 0;
  ast_foreach(args, a) {
    ASSERT(ast_is_type(a, ast_Variable));

    char* fieldName = ast_Identifier_name(ast_Variable_name(a));
    struct TypeField* newField = TypeField_allocate(fieldName);
    struct AstNode* typename = ast_Variable_type(a);
    if(!typename) {
      ERROR_ON_AST(ctx, a, "cannot infer type for '%s'\n", fieldName);
    }
    newField->type = scope_get_Type_from_ast(ctx, sr, typename, 1);
    newField->parentType = t;
    size += newField->type->size;
    LL_APPEND(t->fields, newField);
  }
  t->size = size;
}

static struct ScopeSymbol* build_ScopeSymbol_for_type(
    struct Context* ctx,
    struct ScopeResult* sr,
    struct AstNode* type_ast) {
  ASSERT(ast_is_type(type_ast, ast_Type));
  check_for_redefinition(ctx, sr, type_ast);

  char* name = ast_Identifier_name(ast_Type_name(type_ast));
  struct Type* type = Type_allocate(name);
  struct ScopeSymbol* ss = ScopeSymbol_init_type(type);
  LL_APPEND(sr->symbols, ss);

  if(ast_Type_is_opaque(type_ast)) {
    Type_init_Opaque(type);
  } else {
    if(ast_Type_is_alias(type_ast)) {
      struct AstNode* typename = ast_Type_alias(type_ast);
      struct Type* alias_of = scope_get_Type_from_ast(ctx, sr, typename, 1);
      Type_init_Alias(type, alias_of);
    } else {
      Type_init_Typedef(ctx, sr, type, ast_Type_args(type_ast));
    }
  }
  return ss;
}

static void scope_resolve_internal(
    struct Context* ctx,
    struct ScopeResult* scope,
    struct AstNode* ast);

static struct ScopeSymbol* build_ScopeSymbol_for_func(
    struct Context* ctx,
    struct ScopeResult* sr,
    struct AstNode* func) {
  ASSERT(ast_is_type(func, ast_Function));
  check_for_redefinition(ctx, sr, func);

  // 1. a scope entry for the func name
  // 2. build the arguments into the ScopeFunction entry
  // 3. create a new scope for the func body, insert the arguments into it
  // 4. traverse the body

  // 1.
  struct AstNode* rettype_typename = ast_Function_ret_type(func);
  struct Type* rettype = scope_get_Type_from_ast(ctx, sr, rettype_typename, 1);
  struct ScopeSymbol* func_ss = ScopeSymbol_init_func(func, rettype);
  LL_APPEND(sr->symbols, func_ss);

  if(ast_Function_has_body(func)) {
    // 2.
    ast_foreach_idx(ast_Function_args(func), arg, idx) {
      struct AstNode* typename = ast_Variable_type(arg);
      if(!typename) {
        ERROR_ON_AST(
            ctx,
            arg,
            "cannot infer type for '%s'\n",
            ast_Identifier_name(ast_Variable_name(arg)));
      }
      struct Type* arg_type = scope_get_Type_from_ast(ctx, sr, typename, 1);
      func_ss->ss_function->args[idx] = ScopeSymbol_init_var(arg, arg_type);
    }

    // 3.
    struct AstNode* body = ast_Function_body(func);
    struct ScopeResult* body_scope = allocate_ScopeResult(ctx, body, sr);
    for(int i = 0; i < func_ss->ss_function->num_args; i++) {
      LL_APPEND(body_scope->symbols, func_ss->ss_function->args[i]);
    }
    // 4.
    ast_foreach(ast_Block_stmts(body), s) {
      scope_resolve_internal(ctx, body_scope, s);
    }
  }

  return func_ss;
}

static void scope_resolve_internal(
    struct Context* ctx,
    struct ScopeResult* scope,
    struct AstNode* ast) {
  if(ast_is_type(ast, ast_Variable)) {
    build_ScopeSymbol_for_variable(ctx, scope, ast);
  } else if(ast_is_type(ast, ast_Function)) {
    build_ScopeSymbol_for_func(ctx, scope, ast);
  } else if(ast_is_type(ast, ast_Type)) {
    build_ScopeSymbol_for_type(ctx, scope, ast);
  } else if(ast_is_type(ast, ast_Block)) {
    struct ScopeResult* new_scope = allocate_ScopeResult(ctx, ast, scope);
    ast_foreach(ast_Block_stmts(ast), s) {
      scope_resolve_internal(ctx, new_scope, s);
    }
  } else {
    ast_foreach_child(ast, a) {
      if(a) scope_resolve_internal(ctx, scope, a);
    }
  }
}

char* sst_toString(enum ScopeSymbolType sst) {
  if(sst == sst_Variable) {
    return "variable";
  } else if(sst == sst_Function) {
    return "function";
  } else if(sst == sst_Type) {
    return "type";
  } else if(sst == sst_Builtin) {
    return "builtin";
  } else {
    UNIMPLEMENTED("unknown sst\n");
  }
}

char* ScopeSymbol_name(struct ScopeSymbol* sym) {
  if(sym->sst == sst_Variable) {
    return ast_Identifier_name(ast_Variable_name(sym->ss_variable->variable));
  } else if(sym->sst == sst_Function) {
    return ast_Identifier_name(ast_Function_name(sym->ss_function->function));
  } else if(sym->sst == sst_Type) {
    return sym->ss_type->name;
  } else if(sym->sst == sst_Builtin) {
    UNIMPLEMENTED("unhandled builtin name\n");
  } else {
    UNIMPLEMENTED("unhandled\n");
  }
}
int ScopeSymbol_eq(struct ScopeSymbol* lhs, struct ScopeSymbol* rhs) {
  if(lhs->sst == sst_Variable && rhs->sst == sst_Variable) {
    return lhs->ss_variable->variable == rhs->ss_variable->variable;
  } else if(lhs->sst == sst_Function && rhs->sst == sst_Function) {
    return lhs->ss_function->function == rhs->ss_function->function;
  } else if(lhs->sst == sst_Type && rhs->sst == sst_Type) {
    return Type_eq(lhs->ss_type, rhs->ss_type);
  } else if(lhs->sst == sst_Builtin && rhs->sst == sst_Builtin) {
    UNIMPLEMENTED("unhandled builtin eq\n");
  }

  return 0;
}

struct ScopeResult* scope_lookup(struct Context* ctx, struct AstNode* ast) {
  ASSERT(ast_is_type(ast, ast_Block));
  // lookup existing scopes
  LL_FOREACH(ctx->scope_table, sr) {
    if(sr->ast == ast) {
      return sr;
    }
  }
  return NULL;
}

// BUILTIN(name, size)
// ALIAS(name, alias_to)
// PTR_ALIAS(name, alias_to_basetpy)
#define BUILTIN_TYPES(BUILTIN, ALIAS, PTR_ALIAS)                               \
  BUILTIN(void, 0)                                                             \
  BUILTIN(int64, 8)                                                            \
  BUILTIN(int8, 1)                                                             \
  ALIAS(int, int64)                                                            \
  ALIAS(char, int8)                                                            \
  PTR_ALIAS(string, char)

void install_builtin_types(struct Context* ctx, struct ScopeResult* scope) {

#define ALLOCATE_TYPE(type, name)                                              \
  struct Type* type;                                                           \
  do {                                                                         \
    struct ScopeSymbol* builtin_ss = scope_lookup_name(ctx, scope, name, 0);   \
    ASSERT_MSG(!builtin_ss, "builtin already exists");                         \
    type = Type_allocate(name);                                                \
    builtin_ss = ScopeSymbol_init_type(type);                                  \
    LL_APPEND(scope->symbols, builtin_ss);                                     \
  } while(0)

#define MAKE_BUILTIN(name, size)                                               \
  do {                                                                         \
    ALLOCATE_TYPE(type, #name);                                                \
    Type_init_Builtin(type, size);                                             \
  } while(0);
#define MAKE_ALIAS(name, alias_to)                                             \
  do {                                                                         \
    struct Type* t_alias_to =                                                  \
        scope_get_Type_from_name(ctx, scope, #alias_to, 0);                    \
    ALLOCATE_TYPE(type, #name);                                                \
    Type_init_Alias(type, t_alias_to);                                         \
  } while(0);
#define MAKE_PTR_ALIAS(name, alias_to)                                         \
  do {                                                                         \
    struct Type* t_alias_to =                                                  \
        scope_get_Type_from_name(ctx, scope, #alias_to, 0);                    \
    ALLOCATE_TYPE(type, #name);                                                \
    Type_init_Alias(type, Type_get_ptr_type(t_alias_to));                      \
  } while(0);

  BUILTIN_TYPES(MAKE_BUILTIN, MAKE_ALIAS, MAKE_PTR_ALIAS)
#undef MAKE_BUILTIN
#undef MAKE_ALIAS
#undef MAKE_PTR_ALIAS
#undef ALLOCATE_TYPE
}

void scope_resolve(struct Context* ctx) {
  struct AstNode* body = ctx->ast;
  struct ScopeResult* scope = allocate_ScopeResult(ctx, body, NULL);

  install_builtin_types(ctx, scope);

  ast_foreach(ast_Block_stmts(body), s) {
    scope_resolve_internal(ctx, scope, s);
  }
}
struct ScopeSymbol* scope_lookup_name(
    struct Context* ctx,
    struct ScopeResult* sr,
    char* name,
    int search_parent) {

  LL_FOREACH(sr->symbols, sym) {
    if(strcmp(ScopeSymbol_name(sym), name) == 0) {
      return sym;
    }
  }
  if(search_parent && sr->parent_scope) {
    return scope_lookup_name(ctx, sr->parent_scope, name, search_parent);
  }

  return NULL;
}

struct ScopeSymbol* scope_lookup_typename(
    struct Context* ctx,
    struct ScopeResult* sr,
    struct AstNode* typename,
    int search_parent) {
  ASSERT(ast_is_type(typename, ast_Typename));
  // lookup the name, then make a ptr as needed
  char* name = ast_Typename_name(typename);
  struct ScopeSymbol* base_ss = scope_lookup_name(ctx, sr, name, search_parent);
  if(!base_ss || base_ss->sst != sst_Type) {
    return NULL;
  }

  struct Type* base_type = base_ss->ss_type;
  struct Type* ptr_type = base_type;
  int ptr_level = ast_Typename_ptr_level(typename);
  while(ptr_level != 0) {
    ptr_type = Type_get_ptr_type(ptr_type);
    ptr_level--;
  }
  // dont store the ptr in the symbols
  struct ScopeSymbol* ss = ScopeSymbol_init_type(ptr_type);
  return ss;
}

struct Type* scope_get_Type_from_name(
    struct Context* ctx,
    struct ScopeResult* sr,
    char* name,
    int search_parent) {
  struct ScopeSymbol* sym = scope_lookup_name(ctx, sr, name, search_parent);
  if(!sym || sym->sst != sst_Type) {
    ERROR(ctx, "could not find type named '%s'\n", name);
  }
  return sym->ss_type;
}
struct Type* scope_get_Type_from_ast(
    struct Context* ctx,
    struct ScopeResult* sr,
    struct AstNode* ast,
    int search_parent) {
  if(ast_is_type(ast, ast_Typename)) {
    struct ScopeSymbol* sym =
        scope_lookup_typename(ctx, sr, ast, search_parent);
    if(!sym || sym->sst != sst_Type) {
      ERROR_ON_AST(
          ctx,
          ast,
          "could not find type named '%s'\n",
          ast_Typename_name(ast));
    }
    return sym->ss_type;
  } else if(ast_is_type(ast, ast_Identifier)) {
    char* name = ast_Identifier_name(ast);
    struct ScopeSymbol* sym = scope_lookup_name(ctx, sr, name, search_parent);
    if(sym && sym->sst == sst_Type) {
      return sym->ss_type;
    }
    else if(sym && sym->sst == sst_Variable) {
      return sym->ss_variable->type;
    }
    else {
      ERROR(ctx, "could not find type named '%s'\n", name);
    }
  }
  else if(ast_is_type(ast, ast_Number)) {
    return scope_get_Type_from_name(ctx, sr, "int", 1);
  }
    else if(ast_is_type(ast, ast_String)) {
    return scope_get_Type_from_name(ctx, sr, "string", 1);
  }
  else if(ast_is_type(ast, ast_Expr)) {
    if(ast_Expr_is_plain(ast)) {
      struct Type* type =
          scope_get_Type_from_ast(ctx, sr, ast_Expr_lhs(ast), search_parent);
      return type;
    } else {
      UNIMPLEMENTED("getting type from expr\n");
    }
  } else {
    UNIMPLEMENTED("getting type from ast\n");
  }
}
