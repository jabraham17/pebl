
#include "cg-inst.h"

#include "ast/Type.h"
#include "ast/ast.h"
#include "ast/scope-resolve.h"
#include "builtins/compiler-builtin.h"

#include "cg-helpers.h"

struct cg_value*
codegen_inst(struct Context* ctx, struct AstNode* ast, struct ScopeResult* sr) {

  if(ast_is_type(ast, ast_Function)) {
    return codegen_function(ctx, ast, sr);
  } else if(ast_is_type(ast, ast_Type)) {
    // dont need to do anything for type def
    return NULL;
  } else if(ast_is_type(ast, ast_Expr)) {
    return codegen_expr(ctx, ast, sr);
  } else if(ast_is_type(ast, ast_Assignment)) {

    struct cg_value* lhs = codegen_inst(ctx, ast_Assignment_lhs(ast), sr);
    struct cg_value* rhs = codegen_inst(ctx, ast_Assignment_expr(ast), sr);

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
        codegen_inst(ctx, ast_FieldAccess_object(ast), sr);

    // TODO: much of this logic is now duplicated during scope resolution, we
    // should be able to just  call scope resolve to get types

    struct Type* objectType = Type_get_base_type(object->type);
    struct Type* objectPtrType = NULL;
    if(Type_is_pointer(objectType)) {
      objectPtrType = objectType;
      objectType = Type_get_pointee_type(objectPtrType);
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

      struct cg_value* expr = codegen_inst(ctx, ast_Return_expr(ast), sr);
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
        codegen_inst(ctx, ast_Conditional_condition(ast), sr);
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
      ast_foreach(ast_Block_stmts(body), a) { codegen_inst(ctx, a, if_sr); }
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
          codegen_inst(ctx, a, else_sr);
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
    struct cg_value* expr = codegen_inst(ctx, ast_While_condition(ast), sr);
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
      ast_foreach(ast_Block_stmts(body), a) { codegen_inst(ctx, a, while_sr); }
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
          struct cg_value* init_val = codegen_inst(ctx, init_expr, sr);
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
          struct cg_value* init_val = codegen_inst(ctx, init_expr, sr);
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
