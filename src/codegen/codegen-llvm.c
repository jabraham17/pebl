#include "codegen/codegen-llvm.h"

#include "ast/Type.h"
#include "ast/ast.h"
#include "ast/scope-resolve.h"
#include "builtins/compiler-builtin.h"
#include "common/bsstring.h"
#include "context/arguments.h"
#include "debug/debugwrappers.h"

#include <llvm-c/Analysis.h>
#include <llvm-c/DebugInfo.h>
#include <string.h>

#include "cg-helpers.h"
#include "cg-inst.h"

void init_cg_context(struct Context* ctx) {
  ctx->codegen = malloc(sizeof(*ctx->codegen));
  memset(ctx->codegen, 0, sizeof(*ctx->codegen));
  ctx->codegen->llvmContext = LLVMContextCreate();
}
void deinit_cg_context(struct Context* ctx) {
  LLVMContextDispose(ctx->codegen->llvmContext);
  // TODO
  free(ctx->codegen);
}

static struct cg_function* get_user_main(struct Context* ctx) {
  struct cg_function* user_main = get_function_named(ctx, "main");
  return user_main;
}

static LLVMValueRef codegen_main(struct Context* ctx) {
  // get user main
  struct cg_function* user_main = get_user_main(ctx);
  if(!user_main) {
    // no main, dont create one
    return NULL;
  }

  const char* gen_main_name = "_bs_main_entry";

  // build arg types
  int n_args = 2;
  LLVMTypeRef* params = malloc(sizeof(*params) * n_args);
  params[0] = LLVMPointerTypeInContext(ctx->codegen->llvmContext, 0);
  params[1] = LLVMIntTypeInContext(ctx->codegen->llvmContext, 64);

  // build ret type
  LLVMTypeRef funcType = LLVMFunctionType(
      LLVMIntTypeInContext(ctx->codegen->llvmContext, 64),
      params,
      n_args,
      0);

  LLVMValueRef func =
      LLVMAddFunction(ctx->codegen->module, gen_main_name, funcType);
  LLVMSetLinkage(func, LLVMExternalLinkage);

  LLVMSetValueName(LLVMGetParam(func, 0), "args");
  LLVMSetValueName(LLVMGetParam(func, 1), "nargs");

  // make entry block
  LLVMBasicBlockRef entry =
      LLVMAppendBasicBlockInContext(ctx->codegen->llvmContext, func, "entry");
  LLVMPositionBuilderAtEnd(ctx->codegen->builder, entry);

  LLVMValueRef* args = malloc(sizeof(*args) * n_args);
  for(int i = 0; i < n_args; i++) {
    args[i] = LLVMGetParam(func, i);
  }
  // call user main
  LLVMValueRef call_user_main = LLVMBuildCall2(
      ctx->codegen->builder,
      user_main->cg_type,
      user_main->function,
      args,
      n_args,
      "");
  LLVMBuildRet(ctx->codegen->builder, call_user_main);

  return func;
}

static void set_linkage(struct Context* ctx) {
  struct cg_function* user_main = get_user_main(ctx);

  LL_FOREACH(ctx->codegen->functions, cg_func) {
    // no user main, anything could be external
    if(!user_main) {
      LLVMSetLinkage(cg_func->function, LLVMExternalLinkage);
    } else {
      if(cg_func->is_external) {
        LLVMSetLinkage(cg_func->function, LLVMExternalLinkage);
      } else if(LLVMCountBasicBlocks(cg_func->function) == 0) {
        // if no body def, external
        LLVMSetLinkage(cg_func->function, LLVMExternalLinkage);
      } else {
        // internal
        LLVMSetLinkage(cg_func->function, LLVMInternalLinkage);
      }
    }
  }
}

void codegen(struct Context* ctx) {

  // init the module
  ctx->codegen->module = LLVMModuleCreateWithNameInContext(
      Arguments_outFilename(ctx->arguments),
      ctx->codegen->llvmContext);
  ctx->codegen->builder = LLVMCreateBuilderInContext(ctx->codegen->llvmContext);

  if(Arguments_isDebug(ctx->arguments)) {
    ctx->codegen->debugBuilder = LLVMCreateDIBuilder(ctx->codegen->module);
    ctx->codegen->di.fileUnit = PeblDICreateFile(
        ctx->codegen->debugBuilder,
        Arguments_inFilename(ctx->arguments),
        ".");
    ctx->codegen->di.compileUnit = PeblDICreateCompileUnit(
        ctx->codegen->debugBuilder,
        ctx->codegen->di.fileUnit,
        0,
        NULL);
  }

  ASSERT(ast_is_type(ctx->ast, ast_Block) && ast_next(ctx->ast) == NULL);
  struct AstNode* body = ctx->ast;
  struct ScopeResult* scope = scope_lookup(ctx, body);
  ast_foreach(ast_Block_stmts(body), a) { codegen_inst(ctx, a, scope); }

  codegen_main(ctx);
  set_linkage(ctx);
}
void cg_emit(struct Context* ctx) {

  if(Arguments_isDebug(ctx->arguments)) {
    LLVMDIBuilderFinalize(ctx->codegen->debugBuilder);
  }
  LLVMBool res;
  char* errorMsg;
  res =
      LLVMVerifyModule(ctx->codegen->module, LLVMReturnStatusAction, &errorMsg);
  if(res) {
    WARNING(ctx, "llvm verifification failed\n%s\n", errorMsg);
  }
  res = LLVMPrintModuleToFile(
      ctx->codegen->module,
      Arguments_outFilename(ctx->arguments),
      &errorMsg);
  if(res) {
    ERROR(
        ctx,
        "failed to write output to '%s'\n%s\n",
        Arguments_outFilename(ctx->arguments),
        errorMsg);
  }
}
