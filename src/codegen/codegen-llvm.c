#include "codegen/codegen-llvm.h"

#include "ast/Type.h"
#include "ast/ast.h"
#include "ast/scope-resolve.h"
#include "builtins/compiler-builtin.h"
#include "common/bsstring.h"

#include <llvm-c/Analysis.h>
#include <string.h>

#include "cg-expr.h"
#include "cg-helpers.h"
#include "cg-inst.h"

void init_cg_context(struct Context* ctx, char* filename) {
  ctx->codegen = malloc(sizeof(*ctx->codegen));
  memset(ctx->codegen, 0, sizeof(*ctx->codegen));
  ctx->codegen->llvmContext = LLVMContextCreate();
  ctx->codegen->outfilename = bsstrdup(filename);
}
void deinit_cg_context(struct Context* ctx) {
  LLVMContextDispose(ctx->codegen->llvmContext);
  // TODO
  free(ctx->codegen);
}

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

static int ast_is_constant(struct AstNode* ast) {
  return ast_is_type(ast, ast_Number) || ast_is_type(ast, ast_String);
}
static int ast_is_constant_expr(struct AstNode* ast) {
  if(ast_is_type(ast, ast_Expr)) {
    // currently only allows very simple constants
    if(ast_Expr_is_plain(ast) && ast_is_constant_expr(ast_Expr_lhs(ast)))
      return 1;
    else if(
        ast_Expr_is_binop(ast) && ast_Expr_op(ast) == op_CAST &&
        ast_is_constant_expr(ast_Expr_lhs(ast)))
      return 1;
    else return 0;
  }
  return ast_is_constant(ast);
}

static struct cg_value* codegen_constant_expr(
    struct Context* ctx,
    struct AstNode* ast,
    struct ScopeResult* sr) {
  ASSERT(ast_is_constant_expr(ast));
  if(ast_is_type(ast, ast_Number)) {
    struct Type* t = scope_get_Type_from_ast(ctx, sr, ast, 1);
    LLVMTypeRef cg_type = get_llvm_type(ctx, sr, t);
    LLVMValueRef val =
        LLVMConstInt(cg_type, ast_Number_value(ast), /*signext*/ 1);
    return add_temp_value(ctx, val, cg_type, t);
  } else if(ast_is_type(ast, ast_String)) {
    char* str = ast_String_value(ast);
    return get_string_literal(ctx, sr, str);
  } else if(ast_is_type(ast, ast_Expr)) {
    if(ast_Expr_is_plain(ast) && ast_is_constant_expr(ast_Expr_lhs(ast))) {
      return codegen_constant_expr(ctx, ast_Expr_lhs(ast), sr);
      // TODO: better cast logic
    } else if(
        ast_Expr_is_binop(ast) && ast_Expr_op(ast) == op_CAST &&
        ast_is_constant_expr(ast_Expr_lhs(ast))) {

      struct cg_value* lhsVal =
          codegen_constant_expr(ctx, ast_Expr_lhs(ast), sr);
      struct Type* rhsType =
          scope_get_Type_from_ast(ctx, sr, ast_Expr_rhs(ast), 1);

      struct cg_value* casted = build_const_cast(ctx, sr, lhsVal->type, lhsVal->value, rhsType);
      if(!casted) {
        ERROR_ON_AST(ctx, ast, "no valid cast from '%s' to '%s'\n", Type_to_string(lhsVal->type), Type_to_string(rhsType));
      }
      return casted;

    } else {
      UNIMPLEMENTED("unknown constant expr type\n");
    }
  } else {
    UNIMPLEMENTED("unknown constant type\n");
  }
}

struct cg_value* codegen_helper(
    struct Context* ctx,
    struct AstNode* ast,
    struct ScopeResult* sr) {

  if(ast_is_type(ast, ast_Function)) {
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
          codegen_helper(ctx, elm, body_scope);
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
  } else if(ast_is_type(ast, ast_Type)) {
    // dont need to do anything for type def
    return NULL;
  } else if(ast_is_type(ast, ast_Expr)) {
    return codegen_expr(ctx, ast, sr);
  } else if(ast_is_type(ast, ast_Assignment)) {

    struct cg_value* lhs = codegen_helper(ctx, ast_Assignment_lhs(ast), sr);
    struct cg_value* rhs = codegen_helper(ctx, ast_Assignment_expr(ast), sr);

    LLVMValueRef rhsVal =
        LLVMBuildLoad2(ctx->codegen->builder, rhs->cg_type, rhs->value, "");

    LLVMValueRef ptrToStoreTo;
    if(!ast_Assignment_is_ptr_access(ast)) {
      ptrToStoreTo = lhs->value;
    } else {
      ptrToStoreTo =
          LLVMBuildLoad2(ctx->codegen->builder, lhs->cg_type, lhs->value, "");
    }

    ASSERT(LLVMGetTypeKind(LLVMTypeOf(ptrToStoreTo)) == LLVMPointerTypeKind);
    LLVMBuildStore(ctx->codegen->builder, rhsVal, ptrToStoreTo);

    return NULL;

  } else if(ast_is_type(ast, ast_FieldAccess)) {

    struct cg_value* object =
        codegen_helper(ctx, ast_FieldAccess_object(ast), sr);

    struct Type* objectType = Type_get_base_type(object->type);
    struct Type* objectPtrType = NULL;
    if(Type_is_pointer(objectType)) {
      objectPtrType = objectType;
      objectType = Type_get_base_type(objectPtrType->pointer_to);
    }

    // check types
    if(objectPtrType && !Type_is_pointer(objectPtrType)) {
      ERROR_ON_AST(
          ctx,
          ast,
          "invalid field access - cannot use '->' on a non-pointer\n");
    }
    if(objectType && !Type_is_typedef(objectType)) {
      ERROR_ON_AST(
          ctx,
          ast,
          "invalid field access - base is not a struct-like type\n");
    }

    // compute the offset into the object
    struct TypeField* fieldType = Type_get_TypeField(
        objectType,
        ast_Identifier_name(ast_FieldAccess_field(ast)));
    if(!fieldType) {
      ERROR_ON_AST(ctx, ast, "unknown field name\n");
    }
    int fieldIdx = TypeField_get_index(fieldType);

    LLVMValueRef ptrForGep;
    if(objectPtrType) {
      // load the ptr
      ptrForGep = LLVMBuildLoad2(
          ctx->codegen->builder,
          object->cg_type,
          object->value,
          "");
    } else {
      ptrForGep = object->value;
    }

    // object should already be a ptr, build a gep
    ASSERT(LLVMGetTypeKind(LLVMTypeOf(ptrForGep)) == LLVMPointerTypeKind);

    LLVMTypeRef gepType = get_llvm_type(ctx, sr, objectType);
    LLVMTypeRef idxType = LLVMInt32TypeInContext(ctx->codegen->llvmContext);
    LLVMValueRef gepIdx[2] = {
        LLVMConstInt(idxType, 0, /*signext*/ 1),
        LLVMConstInt(idxType, fieldIdx, /*signext*/ 1)};
    LLVMValueRef gep =
        LLVMBuildGEP2(ctx->codegen->builder, gepType, ptrForGep, gepIdx, 2, "");

    struct cg_value* ret = add_temp_value(
        ctx,
        gep,
        get_llvm_type(ctx, sr, fieldType->type),
        fieldType->type);

    return ret;

  } else if(ast_is_type(ast, ast_Identifier)) {
    struct ScopeSymbol* sym =
        scope_lookup_name(ctx, sr, ast_Identifier_name(ast), 1);
    if(!sym) {
      ERROR_ON_AST(
          ctx,
          ast,
          "failed to scope resolve '%s'\n",
          ast_Identifier_name(ast));
    }
    struct cg_value* val = get_value(ctx, sym);
    if(!val) {
      ERROR_ON_AST(
          ctx,
          ast,
          "failed to find value for '%s'\n",
          ast_Identifier_name(ast));
    }
    return val;

  } else if(ast_is_constant(ast)) {
    struct cg_value* constant = codegen_constant_expr(ctx, ast, sr);
    return allocate_stack_for_temp(
        ctx,
        constant->cg_type,
        constant->value,
        constant->type);
  } else if(ast_is_type(ast, ast_Call)) {
    // get the function we are calling
    struct ScopeSymbol* sym =
        scope_lookup_name(ctx, sr, ast_Identifier_name(ast_Call_name(ast)), 1);
    struct CompilerBuiltin* builtin = NULL;
    if(!sym) {
      // try and get a builtin
      builtin = compiler_builtin_lookup_name(
          ctx,
          ast_Identifier_name(ast_Call_name(ast)));
    }
    if(!sym && !builtin) {
      ERROR_ON_AST(
          ctx,
          ast,
          "could not find function named '%s'\n",
          ast_Identifier_name(ast_Call_name(ast)));
    }

    // if there is a builtin, do codegen for it
    if(builtin) {
      struct cg_value* val = codegenBuiltin(ctx, sr, builtin, ast);
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
      struct cg_value* val = codegen_helper(ctx, a, sr);
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

  } else if(ast_is_type(ast, ast_Return)) {

    if(ast_Return_expr(ast)) {

      struct cg_value* expr = codegen_helper(ctx, ast_Return_expr(ast), sr);
      ASSERT_MSG(
          LLVMGetTypeKind(LLVMTypeOf(expr->value)) == LLVMPointerTypeKind,
          "cannot load a non-pointer value\n");
      // load the expr and return it
      LLVMValueRef load =
          LLVMBuildLoad2(ctx->codegen->builder, expr->cg_type, expr->value, "");
      LLVMValueRef ret = LLVMBuildRet(ctx->codegen->builder, load);
      return add_temp_value(
          ctx,
          ret,
          LLVMTypeOf(ret),
          NULL /*TODO: i should be able to look up the type of `expr`*/);
    } else {
      LLVMValueRef ret = LLVMBuildRetVoid(ctx->codegen->builder);
      return add_temp_value(ctx, ret, LLVMTypeOf(ret), NULL);
    }
  } else if(ast_is_type(ast, ast_Conditional)) {

    int has_else = !ast_Block_is_empty(ast_Conditional_else_body(ast));

    LLVMBasicBlockRef currBB = LLVMGetInsertBlock(ctx->codegen->builder);
    LLVMValueRef currentFunc = LLVMGetBasicBlockParent(currBB);
    // create BBs
    LLVMBasicBlockRef thenBB =
        LLVMCreateBasicBlockInContext(ctx->codegen->llvmContext, "if.body");
    LLVMBasicBlockRef elseBB = NULL;
    if(has_else) {
      elseBB =
          LLVMCreateBasicBlockInContext(ctx->codegen->llvmContext, "if.else");
    }
    LLVMBasicBlockRef endBB =
        LLVMCreateBasicBlockInContext(ctx->codegen->llvmContext, "if.end");

    // get the expr and build branch
    struct cg_value* expr =
        codegen_helper(ctx, ast_Conditional_condition(ast), sr);
    LLVMValueRef exprVal =
        LLVMBuildLoad2(ctx->codegen->builder, expr->cg_type, expr->value, "");
    LLVMValueRef cond = LLVMBuildICmp(
        ctx->codegen->builder,
        LLVMIntNE,
        exprVal,
        LLVMConstNull(LLVMTypeOf(exprVal)),
        "");
    LLVMBuildCondBr(
        ctx->codegen->builder,
        cond,
        thenBB,
        has_else ? elseBB : endBB);

    // build the body
    LLVMAppendExistingBasicBlock(currentFunc, thenBB);
    LLVMPositionBuilderAtEnd(ctx->codegen->builder, thenBB);

    // get the scope for the if block and codegen it
    // only codegen body if it exists
    struct AstNode* body = ast_Conditional_if_body(ast);
    struct ScopeResult* if_sr = scope_lookup(ctx, body);
    if(!ast_Block_is_empty(body)) {
      ast_foreach(ast_Block_stmts(body), a) { codegen_helper(ctx, a, if_sr); }
    }
    // if previous inst was a terminator, dont add one here
    if(!LLVMGetBasicBlockTerminator(
           LLVMGetInsertBlock(ctx->codegen->builder))) {
      // create br to endBB
      LLVMBuildBr(ctx->codegen->builder, endBB);
    }

    // if else, get the scope for it and build it
    if(has_else) {
      LLVMAppendExistingBasicBlock(currentFunc, elseBB);
      LLVMPositionBuilderAtEnd(ctx->codegen->builder, elseBB);

      struct AstNode* else_body = ast_Conditional_else_body(ast);
      struct ScopeResult* else_sr = scope_lookup(ctx, else_body);
      if(!ast_Block_is_empty(else_body)) {
        ast_foreach(ast_Block_stmts(else_body), a) {
          codegen_helper(ctx, a, else_sr);
        }

        // if previous inst was a terminator, dont add one here
        if(!LLVMGetBasicBlockTerminator(
               LLVMGetInsertBlock(ctx->codegen->builder))) {
          // create br to endBB
          LLVMBuildBr(ctx->codegen->builder, endBB);
        }
      }
    }

    // move to end and keep going
    LLVMAppendExistingBasicBlock(currentFunc, endBB);
    LLVMPositionBuilderAtEnd(ctx->codegen->builder, endBB);
    return NULL;
  } else if(ast_is_type(ast, ast_While)) {

    LLVMBasicBlockRef currBB = LLVMGetInsertBlock(ctx->codegen->builder);
    LLVMValueRef currentFunc = LLVMGetBasicBlockParent(currBB);
    // create BBs
    LLVMBasicBlockRef condBB =
        LLVMCreateBasicBlockInContext(ctx->codegen->llvmContext, "while.cond");
    LLVMBasicBlockRef bodyBB =
        LLVMCreateBasicBlockInContext(ctx->codegen->llvmContext, "while.body");
    LLVMBasicBlockRef endBB =
        LLVMCreateBasicBlockInContext(ctx->codegen->llvmContext, "while.end");

    // branch to the cond
    LLVMBuildBr(ctx->codegen->builder, condBB);

    // build the cond
    LLVMAppendExistingBasicBlock(currentFunc, condBB);
    LLVMPositionBuilderAtEnd(ctx->codegen->builder, condBB);

    // get the expr and build branch
    struct cg_value* expr = codegen_helper(ctx, ast_While_condition(ast), sr);
    LLVMValueRef exprVal =
        LLVMBuildLoad2(ctx->codegen->builder, expr->cg_type, expr->value, "");
    LLVMValueRef cond = LLVMBuildICmp(
        ctx->codegen->builder,
        LLVMIntNE,
        exprVal,
        LLVMConstNull(LLVMTypeOf(exprVal)),
        "");
    LLVMBuildCondBr(ctx->codegen->builder, cond, bodyBB, endBB);

    // build the body
    LLVMAppendExistingBasicBlock(currentFunc, bodyBB);
    LLVMPositionBuilderAtEnd(ctx->codegen->builder, bodyBB);

    // get the scope for the if block and codegen it
    // only codegen body if it exists
    struct AstNode* body = ast_While_body(ast);
    struct ScopeResult* while_sr = scope_lookup(ctx, body);
    if(!ast_Block_is_empty(body)) {
      ast_foreach(ast_Block_stmts(body), a) {
        codegen_helper(ctx, a, while_sr);
      }
    }
    // if previous inst was a terminator, dont add one here
    if(!LLVMGetBasicBlockTerminator(
           LLVMGetInsertBlock(ctx->codegen->builder))) {
      // create br to cond
      LLVMBuildBr(ctx->codegen->builder, condBB);
    }

    // move to end and keep going
    LLVMAppendExistingBasicBlock(currentFunc, endBB);
    LLVMPositionBuilderAtEnd(ctx->codegen->builder, endBB);
    return NULL;
  } else if(ast_is_type(ast, ast_Break)) {
    UNIMPLEMENTED("break is not yet implemented\n");
  } else if(ast_is_type(ast, ast_Variable)) {

    // lookup sym
    struct ScopeSymbol* sym = scope_lookup_name(
        ctx,
        sr,
        ast_Identifier_name(ast_Variable_name(ast)),
        1);
    if(sym == NULL) {
      ERROR_ON_AST(
          ctx,
          ast,
          "could not resolve '%s'\n",
          ast_Identifier_name(ast_Variable_name(ast)));
    }
    ASSERT(sym->sst == sst_Variable);
    LLVMTypeRef cg_type = get_llvm_type_sym(ctx, sr, sym);
    struct AstNode* init_expr = ast_Variable_expr(ast);

    int is_global = sr->parent_scope == NULL;
    if(is_global) {
      char* name = ast_Identifier_name(ast_Variable_name(ast));
      LLVMValueRef global = LLVMAddGlobal(ctx->codegen->module, cg_type, name);
      LLVMSetLinkage(global, LLVMPrivateLinkage);
      if(init_expr) {
        if(ast_is_constant_expr(init_expr)) {
          struct cg_value* init_val = codegen_constant_expr(ctx, init_expr, sr);
          LLVMSetInitializer(global, init_val->value);
        } else {
          ERROR_ON_AST(
              ctx,
              ast,
              "cannot have a global variable with non-constant init "
              "expression\n");
        }
      }
      struct cg_value* value = add_value(ctx, global, cg_type, sym);
      return value;

    } else {
      if(LLVMGetTypeKind(cg_type) == LLVMVoidTypeKind) {
        if(init_expr) {
          // just build the init expression, we are not saving the value
          struct cg_value* init_val = codegen_helper(ctx, init_expr, sr);
          return init_val;
        } else {
          ERROR_ON_AST(
              ctx,
              ast,
              "cannot have a variable with void type and no init\n");
        }
      } else {
        struct cg_value* val;
        if(init_expr) {
          struct cg_value* init_val = codegen_helper(ctx, init_expr, sr);
          // load the init value, store it to the new variable
          ASSERT_MSG(
              LLVMGetTypeKind(LLVMTypeOf(init_val->value)) ==
                  LLVMPointerTypeKind,
              "cannot load a non-pointer value\n");
          LLVMValueRef load = LLVMBuildLoad2(
              ctx->codegen->builder,
              init_val->cg_type,
              init_val->value,
              "");
          val = allocate_stack_for_sym(ctx, cg_type, load, sym);
        } else {
          val =
              allocate_stack_for_sym(ctx, cg_type, LLVMGetUndef(cg_type), sym);
        }

        return val;
      }
    }
  } else {
    UNIMPLEMENTED("not handled\n");
  }
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
      ctx->codegen->outfilename,
      ctx->codegen->llvmContext);
  ctx->codegen->builder = LLVMCreateBuilderInContext(ctx->codegen->llvmContext);

  ASSERT(ast_is_type(ctx->ast, ast_Block) && ast_next(ctx->ast) == NULL);
  struct AstNode* body = ctx->ast;
  struct ScopeResult* scope = scope_lookup(ctx, body);
  ast_foreach(ast_Block_stmts(body), a) { codegen_helper(ctx, a, scope); }

  codegen_main(ctx);
  set_linkage(ctx);
}
void cg_emit(struct Context* ctx) {
  LLVMBool res;
  char* errorMsg;
  res =
      LLVMVerifyModule(ctx->codegen->module, LLVMReturnStatusAction, &errorMsg);
  if(res) {
    WARNING(ctx, "llvm verifification failed\n%s\n", errorMsg);
  }
  res = LLVMPrintModuleToFile(
      ctx->codegen->module,
      ctx->codegen->outfilename,
      &errorMsg);
  if(res) {
    ERROR(
        ctx,
        "failed to write output to '%s'\n%s\n",
        ctx->codegen->outfilename,
        errorMsg);
  }
}
