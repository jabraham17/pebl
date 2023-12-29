
#include "Type.h"
#include "ast.h"

// scope resolve ast, building a ScopeResult object

enum ScopeSymbolType { sst_Variable, sst_Function, sst_Type, sst_Builtin };
char* sst_toString(enum ScopeSymbolType sst);

struct ScopeSymbol;

struct ScopeVariable {
  struct AstNode* variable;
  struct Type* type;
};
struct ScopeFunction {
  struct AstNode* function;
  struct Type* rettype;
  int num_args;
  struct ScopeSymbol** args;
};
struct ScopeBuiltin {};

struct ScopeSymbol {
  enum ScopeSymbolType sst;
  struct ScopeSymbol* next;

  struct ScopeVariable* ss_variable;
  struct ScopeFunction* ss_function;
  struct Type* ss_type;
  struct ScopeBuiltin* ss_builtin;
};

char* ScopeSymbol_name(struct ScopeSymbol* sym);
int ScopeSymbol_eq(struct ScopeSymbol* lhs, struct ScopeSymbol* rhs);

struct ScopeResult {
  struct AstNode* ast;
  struct ScopeSymbol* symbols;

  struct ScopeResult* parent_scope;
  struct ScopeResult* next;
};

void scope_resolve(struct Context* ctx);
struct ScopeResult* scope_lookup(struct Context* ctx, struct AstNode* ast);

struct ScopeSymbol* scope_lookup_name(
    struct Context* ctx,
    struct ScopeResult* sr,
    char* name,
    int search_parent);

struct ScopeSymbol* scope_lookup_typename(
    struct Context* ctx,
    struct ScopeResult* sr,
    struct AstNode* typename,
    int search_parent);

struct Type* scope_get_Type_from_name(
    struct Context* ctx,
    struct ScopeResult* sr,
    char* name,
    int search_parent);
struct Type* scope_get_Type_from_ast(
    struct Context* ctx,
    struct ScopeResult* sr,
    struct AstNode* ast,
    int search_parent);
