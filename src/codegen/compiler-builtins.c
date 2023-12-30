
#include "ast/Type.h"
#include "ast/scope-resolve.h"
#include "builtins/compiler-builtin.h"
#include "codegen/codegen-llvm.h"
#include "common/bsstring.h"
#include "common/ll-common.h"

#include <llvm-c/Core.h>
#include <stdlib.h>
#include <string.h>

#include "cg-expr.h"
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

static int validArgumentToSizeof(struct AstNode* arg) {
  if(arg == NULL) return 0;
  if(ast_is_type(arg, ast_Identifier)) return 1;
  if(ast_is_type(arg, ast_Typename)) return 1;
  if(ast_is_type(arg, ast_Expr) && ast_Expr_is_plain(arg)) {
    return validArgumentToSizeof(ast_Expr_lhs(arg));
  }
  return 0;
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
  if(!validArgumentToSizeof(arg)) {
    ERROR_ON_AST(ctx, call, "illegal usage of sizeof()\n");
  }
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

static struct cg_value* codegenBuiltin_codegenTypeof(
    struct Context* ctx,
    struct ScopeResult* scope,
    __attribute__((unused)) struct CompilerBuiltin* builtin,
    struct AstNode* call) {
  ASSERT(ast_is_type(call, ast_Call));

  // call should have one arg
  if(ast_Call_num_args(call) != 1) {
    ERROR_ON_AST(ctx, call, "typeof() expects 1 argument\n");
  }
  struct AstNode* arg = ast_Call_args(call);
  struct Type* type = scope_get_Type_from_ast(ctx, scope, arg, 1);
  if(!type) {
    ERROR_ON_AST(ctx, call, "could not find type for typeof()\n");
  }

  char* typename = Type_to_string(type);

  struct cg_value* str_lit = get_string_literal(ctx, scope, typename);
  return allocate_stack_for_temp(
      ctx,
      str_lit->cg_type,
      str_lit->value,
      str_lit->type);
}

static struct cg_value* codegenBuiltin_codegenAssert(
    struct Context* ctx,
    struct ScopeResult* scope,
    __attribute__((unused)) struct CompilerBuiltin* builtin,
    struct AstNode* call) {
  ASSERT(ast_is_type(call, ast_Call));

  // call should have one arg, an expression
  if(ast_Call_num_args(call) != 1) {
    ERROR_ON_AST(ctx, call, "assert() expects 1 argument\n");
  }
  struct AstNode* arg = ast_Call_args(call);
  if(!ast_is_type(arg, ast_Expr)) {
    ERROR_ON_AST(ctx, call, "illegal usage of assert()\n");
  }

  // build a conditional trap
  LLVMBasicBlockRef currBB = LLVMGetInsertBlock(ctx->codegen->builder);
  LLVMValueRef currentFunc = LLVMGetBasicBlockParent(currBB);
  // create BBs
  LLVMBasicBlockRef thenBB =
      LLVMCreateBasicBlockInContext(ctx->codegen->llvmContext, "assert");
  LLVMBasicBlockRef endBB =
      LLVMCreateBasicBlockInContext(ctx->codegen->llvmContext, "");

  struct cg_value* expr = codegen_expr(ctx, arg, scope);
  LLVMValueRef exprVal =
      LLVMBuildLoad2(ctx->codegen->builder, expr->cg_type, expr->value, "");

  LLVMValueRef cond = LLVMBuildICmp(
      ctx->codegen->builder,
      LLVMIntEQ,
      exprVal,
      LLVMConstNull(LLVMTypeOf(exprVal)),
      "");
  LLVMBuildCondBr(ctx->codegen->builder, cond, thenBB, endBB);

  // build the body
  LLVMAppendExistingBasicBlock(currentFunc, thenBB);
  LLVMPositionBuilderAtEnd(ctx->codegen->builder, thenBB);

  // the body is just a trap
  char* name = "llvm.debugtrap";
  unsigned ID = LLVMLookupIntrinsicID(name, strlen(name));
  LLVMTypeRef intrinsicTy =
      LLVMIntrinsicGetType(ctx->codegen->llvmContext, ID, NULL, 0);
  LLVMValueRef intrinsic =
      LLVMGetIntrinsicDeclaration(ctx->codegen->module, ID, NULL, 0);
  LLVMBuildCall2(ctx->codegen->builder, intrinsicTy, intrinsic, NULL, 0, "");

  // create br to endBB
  LLVMBuildBr(ctx->codegen->builder, endBB);

  // move to end and keep going
  LLVMAppendExistingBasicBlock(currentFunc, endBB);
  LLVMPositionBuilderAtEnd(ctx->codegen->builder, endBB);

  // return poison
  LLVMTypeRef poisonType =
      LLVMPointerTypeInContext(ctx->codegen->llvmContext, 0);
  return allocate_stack_for_temp(
      ctx,
      poisonType,
      LLVMGetPoison(poisonType),
      NULL);
}

struct cg_value* codegenBuiltin(
    struct Context* ctx,
    struct ScopeResult* scope,
    struct CompilerBuiltin* builtin,
    struct AstNode* call) {

#define COMPILER_BUILTIN(name_, numArgs_, codegenFunc)                         \
  if(strcmp(#name_, builtin->name) == 0 && numArgs_ == builtin->numArgs) {     \
    return codegenBuiltin_##codegenFunc(ctx, scope, builtin, call);            \
  }
#include "compiler-builtins.def"

  return NULL;
}
