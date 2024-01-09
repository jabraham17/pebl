#include "ast/ast.h"
#include "ast/scope-resolve.h"
#include "builtins/compiler-builtin.h"

#include <llvm-c/DebugInfo.h>

#include "cg-helpers.h"
#include "cg-inst.h"

struct cg_value*
codegen_call(struct Context* ctx, struct AstNode* ast, struct ScopeResult* sr) {

  ASSERT(ast_is_type(ast, ast_Call));
  // get the function we are calling
  struct ScopeSymbol* sym =
      scope_lookup_name(ctx, sr, ast_Identifier_name(ast_Call_name(ast)), 1);
  if(!sym) {
    ERROR_ON_AST(
        ctx,
        ast,
        "could not find function named '%s'\n",
        ast_Identifier_name(ast_Call_name(ast)));
  }

  // if there is a builtin, do codegen for it
  if(ScopeSymbol_isBuiltin(sym)) {
    struct cg_value* val = codegenBuiltin(ctx, sr, sym, ast);
    if(!val) {
      struct Location* loc = Context_get_location(ctx, ast);
      int lineno = -1;
      if(loc) {
        lineno = loc->line_start;
      }
      UNIMPLEMENTED("unimplemented builtin on line %d\n", lineno);
    }
    return val;
  }

  char* mname = mangled_name(ctx, ast);
  // get the function
  struct cg_function* func = get_function_named(ctx, mname);
  if(!func) {
    ERROR_ON_AST(ctx, ast, "could not find function named '%s'\n", mname);
  }
  // TODO: theres a bunch of checks that should happen to make sure funcs
  // arent called wrong. but thats more a scope resolve/type checking issue

  int numArgs = ast_Call_num_args(ast);
  LLVMValueRef* args = malloc(sizeof(*args) * numArgs);
  ast_foreach_idx(ast_Call_args(ast), a, i) {
    struct cg_value* val = codegen_inst(ctx, a, sr);
    args[i] =
        LLVMBuildLoad2(ctx->codegen->builder, val->cg_type, val->value, "");
  }
  LLVMValueRef call = LLVMBuildCall2(
      ctx->codegen->builder,
      func->cg_type,
      func->function,
      args,
      numArgs,
      "");

  if(Arguments_isDebug(ctx->arguments)) {

    LLVMBasicBlockRef currentBB = LLVMGetInsertBlock(ctx->codegen->builder);
    LLVMValueRef currentFunc =
        currentBB ? LLVMGetBasicBlockParent(currentBB) : NULL;
    LLVMMetadataRef currentSP =
        currentFunc ? LLVMGetSubprogram(currentFunc) : NULL;
    if(currentSP) {
      LLVMMetadataRef loc = LLVMDIBuilderCreateDebugLocation(
          ctx->codegen->llvmContext,
          0,
          0,
          currentSP,
          NULL);
      LLVMInstructionSetDebugLoc(call, loc);
    }
  }

  LLVMTypeRef rettype = LLVMGetReturnType(func->cg_type);
  if(LLVMGetTypeKind(rettype) != LLVMVoidTypeKind) {
    return allocate_stack_for_temp(
        ctx,
        LLVMGetReturnType(func->cg_type),
        call,
        func->rettype);
  } else {
    LLVMTypeRef poisonType =
        LLVMPointerTypeInContext(ctx->codegen->llvmContext, 0);
    return allocate_stack_for_temp(
        ctx,
        poisonType,
        LLVMGetPoison(poisonType),
        func->rettype);
  }
}
