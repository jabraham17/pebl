#include "ast/Type.h"
#include "ast/ast.h"
#include "ast/scope-resolve.h"

#include <llvm-c/DebugInfo.h>
#include <string.h>

#include "cg-helpers.h"
#include "cg-inst.h"

static struct cg_function* codegen_function_prototype(
    struct Context* ctx,
    struct AstNode* ast_func,
    struct ScopeResult* surroundScope) {
  ASSERT(ast_is_type(ast_func, ast_Function));

  char* name = ast_Identifier_name(ast_Function_name(ast_func));
  char* mname = mangled_name(ctx, ast_func);

  LLVMValueRef func = LLVMGetNamedFunction(ctx->codegen->module, mname);
  struct cg_function* cg_func = NULL;
  if(func) {
    // already exists
    cg_func = get_function_named(ctx, mname);
  } else {
    // build arg types
    int n_args = ast_Function_num_args(ast_func);
    LLVMTypeRef* params = malloc(sizeof(*params) * n_args);
    ast_foreach_idx(ast_Function_args(ast_func), arg, i) {
      if(!ast_Variable_type(arg)) {
        ERROR_ON_AST(
            ctx,
            arg,
            "cannot infer type of '%s'\n",
            ast_Identifier_name(ast_Variable_name(arg)));
      }
      params[i] = get_llvm_type_ast(ctx, surroundScope, ast_Variable_type(arg));
    }
    // build ret type
    LLVMTypeRef funcType = LLVMFunctionType(
        get_llvm_type_ast(ctx, surroundScope, ast_Function_ret_type(ast_func)),
        params,
        n_args,
        0);

    // build function
    func = LLVMAddFunction(ctx->codegen->module, mname, funcType);
    cg_func = add_function(
        ctx,
        name,
        mname,
        func,
        funcType,
        scope_get_Type_from_ast(
            ctx,
            surroundScope,
            ast_Function_ret_type(ast_func),
            1));

    if(ast_Function_is_extern(ast_func) || ast_Function_is_export(ast_func)) {
      cg_func->is_external = 1;
    }

    if(Arguments_isDebug(ctx->arguments)) {
      LLVMMetadataRef PPP[] = {NULL};

      cg_func->di.subprogram = LLVMDIBuilderCreateFunction(
          ctx->codegen->debugBuilder,
          ctx->codegen->di.fileUnit,
          name,
          strlen(name),
          mname,
          strlen(mname),
          ctx->codegen->di.fileUnit,
          0,
          LLVMDIBuilderCreateSubroutineType(
              ctx->codegen->debugBuilder,
              ctx->codegen->di.fileUnit,
              PPP,
              1,
              0),
          !cg_func->is_external,
          ast_Function_has_body(ast_func),
          0,
          LLVMDIFlagPrototyped,
          0);
      LLVMSetSubprogram(func, cg_func->di.subprogram);
    }

    // create names for the parameters
    ast_foreach_idx(ast_Function_args(ast_func), arg, i) {

      struct AstNode* var = ast_Variable_name(arg);
      char* varname = ast_Identifier_name(var);

      LLVMValueRef param = LLVMGetParam(func, i);
      LLVMSetValueName(param, varname);
    }
  }

  return cg_func;
}

struct cg_value* codegen_function(
    struct Context* ctx,
    struct AstNode* ast,
    struct ScopeResult* sr) {
  ASSERT(ast_is_type(ast, ast_Function));
  struct cg_function* func = codegen_function_prototype(ctx, ast, sr);
  // generate body
  if(ast_Function_has_body(ast)) {
    struct AstNode* body = ast_Function_body(ast);
    struct ScopeResult* body_scope = scope_lookup(ctx, body);
    // make entry block
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(
        ctx->codegen->llvmContext,
        func->function,
        "entry");
    LLVMPositionBuilderAtEnd(ctx->codegen->builder, entry);

    // copy all the parameters to the stack
    ast_foreach_idx(ast_Function_args(ast), arg, i) {

      struct AstNode* var = ast_Variable_name(arg);
      char* varname = ast_Identifier_name(var);

      struct ScopeSymbol* sym =
          scope_lookup_name(ctx, body_scope, ast_Identifier_name(var), 1);
      if(sym == NULL) {
        ERROR_ON_AST(ctx, arg, "could not resolve '%s'\n", varname);
      }
      LLVMValueRef param_val = LLVMGetParam(func->function, i);
      LLVMTypeRef param_type = LLVMTypeOf(param_val);

      // allocate state space
      LLVMValueRef stack_ptr =
          LLVMBuildAlloca(ctx->codegen->builder, param_type, "");
      add_value(ctx, stack_ptr, param_type, sym);

      // store param to stack
      LLVMBuildStore(ctx->codegen->builder, param_val, stack_ptr);
    }
    // codegen body

    if(!ast_Block_is_empty(body)) {
      ast_foreach(ast_Block_stmts(body), elm) {
        codegen_inst(ctx, elm, body_scope);
      }
    }

    // after codegening the body, make sure there is a final ret
    if(!LLVMGetBasicBlockTerminator(
           LLVMGetInsertBlock(ctx->codegen->builder))) {
      LLVMTypeRef rettype = LLVMGetReturnType(func->cg_type);
      if(LLVMGetTypeKind(rettype) != LLVMVoidTypeKind) {
        LLVMBuildRet(ctx->codegen->builder, LLVMGetPoison(rettype));
      } else {
        LLVMBuildRetVoid(ctx->codegen->builder);
      }
    }
  }
  return add_temp_value(ctx, func->function, func->cg_type, func->rettype);
}
