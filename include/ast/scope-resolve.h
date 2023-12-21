
#include "TypeTable.h"
#include "ast.h"

// scope resolve ast, building a ScopeResult object

struct ScopeSymbol {
  // common
  char* name;
  struct Type* type;
  int is_var;

  int num_args;
  struct ScopeSymbol** args;

  struct ScopeSymbol* next;
};
char* ScopeSymbol_name(struct ScopeSymbol* sym);

int ScopeSymbol_is_variable(struct ScopeSymbol* sym);
struct Type* ScopeSymbol_type(struct ScopeSymbol* sym);

int ScopeSymbol_is_function(struct ScopeSymbol* sym);
struct Type* ScopeSymbol_return_type(struct ScopeSymbol* sym);
int ScopeSymbol_num_args(struct ScopeSymbol* sym);
struct ScopeSymbol* ScopeSymbol_arg(struct ScopeSymbol* sym, int i);
int ScopeSymbol_eq(struct ScopeSymbol* lhs, struct ScopeSymbol* rhs);

struct ScopeResult {
  struct AstNode* scope_for;
  struct ScopeSymbol* variables;

  struct ScopeResult* parent_scope;
  struct ScopeResult* next;
};

// only lookup existing scopes
struct ScopeResult*
scope_lookup(struct Context* context, struct AstNode* scope_for);
// lookup a single ident
struct ScopeSymbol* scope_lookup_identifier(
    struct Context* context,
    struct ScopeResult* sr,
    struct AstNode* ident,
    int search_parent);
// fresly scope resolve, returns scope if already resolved
struct ScopeResult*
scope_resolve(struct Context* context, struct AstNode* scope_for);
