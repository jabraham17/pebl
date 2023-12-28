
#include "ast/Type.h"
#include "builtins/compiler-builtin.h"
#include "codegen/codegen-llvm.h"
#include "common/bsstring.h"
#include "common/ll-common.h"
#include "ast/scope-resolve.h"

#include <llvm-c/Core.h>
#include <stdlib.h>
#include <string.h>

#include "cg-helpers.h"

static struct CompilerBuiltin*
allocate_Builtin(struct Context* ctx, char* name, int numArgs) {

  struct CompilerBuiltin* newBuiltin = malloc(sizeof(*newBuiltin));
  memset(newBuiltin, 0, sizeof(*newBuiltin));
  newBuiltin->name = bsstrdup(name);
  newBuiltin->numArgs = numArgs;
  LL_APPEND(ctx->compiler_builtins, newBuiltin);
  return newBuiltin;
}

void register_compiler_builtins(struct Context* ctx) {

#define COMPILER_BUILTIN(name, numArgs, codegenFunc)                           \
  do {                                                                         \
    allocate_Builtin(ctx, #name, numArgs);                                     \
  } while(0);
#include "compiler-builtins.def"
}

struct CompilerBuiltin*
compiler_builtin_lookup_name(struct Context* ctx, char* name) {
  if(!ctx->compiler_builtins) return NULL;
  LL_FOREACH(ctx->compiler_builtins, cb) {
    if(strcmp(cb->name, name) == 0) return cb;
  }
  return NULL;
}


static struct cg_value* codegenBuiltin_codegenSizeof(
    struct Context* ctx,
    struct ScopeResult* scope,
    __attribute__((unused)) struct CompilerBuiltin* builtin,
    struct AstNode* call) {
  ASSERT(ast_is_type(call, ast_Call));

  // call should have one arg, an identifier representing a typename
  if(ast_Call_num_args(call) != 1) {
    ERROR_ON_AST(ctx, call, "sizeof() expects 1 argument\n");
  }
  struct AstNode* arg = ast_Call_args(call);
  struct Type* type = scope_get_Type_from_ast(ctx, scope, arg, 1);
  if(!type) {
    ERROR_ON_AST(ctx, call, "could not find type for sizeof()\n");
  }
  int size = Type_get_size(type);

  LLVMTypeRef cg_type = LLVMInt64TypeInContext(ctx->codegen->llvmContext);
  LLVMValueRef val = LLVMConstInt(cg_type, size, /*signext*/ 1);
  return allocate_stack_for_temp(
      ctx,
      cg_type,
      val,
      scope_get_Type_from_name(ctx, scope, "int", 1));
}

struct cg_value* codegenBuiltin(
    struct Context* ctx,
    struct ScopeResult* scope,
    struct CompilerBuiltin* builtin,
    struct AstNode* call) {

#define COMPILER_BUILTIN(name_, numArgs_, codegenFunc)                         \
  if(strcmp(#name_, builtin->name) == 0 && numArgs_ == builtin->numArgs) {     \
    return codegenBuiltin_##codegenFunc(ctx, scope, builtin, call);                   \
  }
#include "compiler-builtins.def"

  return NULL;
}
