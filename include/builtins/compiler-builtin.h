#ifndef COMPILER_BUILTINS_H_
#define COMPILER_BUILTINS_H_

#include "ast/ast.h"
#include "context/context.h"

struct CompilerBuiltin {
  char* name;
  int numArgs;
  struct CompilerBuiltin* next;
};

void register_compiler_builtins(struct Context* ctx);

struct CompilerBuiltin*
compiler_builtin_lookup_name(struct Context* ctx, char* name);

struct cg_value;
struct cg_value* codegenBuiltin(
    struct Context* ctx,
    struct CompilerBuiltin* builtin,
    struct AstNode* call);

#endif
