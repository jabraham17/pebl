

#include "cg-helpers.h"

#include "ast/TypeTable.h"
#include "ast/ast.h"
#include "ast/scope-resolve.h"
#include "common/bsstring.h"

#include <string.h>

void clear_current_values(struct Context* ctx) {
  ctx->codegen->current_values = NULL;
  // TODO: proper clear
}
struct cg_value* get_value(struct Context* ctx, struct ScopeSymbol* ss) {
  LL_FOREACH(ctx->codegen->current_values, v) {
    if(v->variable && ScopeSymbol_eq(v->variable, ss)) {
      return v;
    }
  }
  // no value
  return NULL;
}
struct cg_value* add_temp_value(
    struct Context* ctx,
    LLVMValueRef value,
    LLVMTypeRef cg_type,
    struct Type* type) {
  struct cg_value* val = malloc(sizeof(*val));
  memset(val, 0, sizeof(*val));
  val->value = value;
  val->cg_type = cg_type;
  val->type = type;
  LL_APPEND(ctx->codegen->current_values, val);
  return val;
}
struct cg_value* add_value(
    struct Context* ctx,
    LLVMValueRef value,
    LLVMTypeRef cg_type,
    struct ScopeSymbol* ss,
    struct Type* type) {
  struct cg_value* val = add_temp_value(ctx, value, cg_type, type);
  val->variable = ss;
  return val;
}

struct cg_function* get_function_named(struct Context* ctx, char* mname) {
  LL_FOREACH(ctx->codegen->functions, f) {
    if(strcmp(f->mname, mname) == 0) {
      return f;
    }
  }
  return NULL;
}
struct cg_function* add_function(
    struct Context* ctx,
    char* name,
    char* mname,
    LLVMValueRef func,
    LLVMTypeRef cg_type,
    struct Type* rettype) {
  struct cg_function* f = malloc(sizeof(*f));
  memset(f, 0, sizeof(*f));
  f->name = bsstrdup(name);
  f->mname = bsstrdup(mname);
  f->function = func;
  f->cg_type = cg_type;
  f->rettype = rettype;
  LL_APPEND(ctx->codegen->functions, f);
  return f;
}

char* mangled_name(struct Context* ctx, struct AstNode* ast) {
  // TODO: implement name mangling
  if(ast_is_type(ast, ast_Function)) {
    char* name = ast_Identifier_name(ast_Function_name(ast));
    return name;
    // if (ast_Function_is_extern(ast) || ast_Function_is_export(ast)) {
    //   return name;
    // } else {
    //   char *mangled = bsstrcat("bs_", name);
    //   return mangled;
    // }
  } else if(ast_is_type(ast, ast_Identifier)) {
    char* name = ast_Identifier_name(ast);
    return name;
    // char *mangled = bsstrcat("bs_", name);
    // return mangled;
  } else if(ast_is_type(ast, ast_Call)) {
    char* name = ast_Identifier_name(ast_Call_name(ast));
    return name;
  } else {
    ERROR(ctx, "illegal usage of 'mangled_name'\n");
  }
}

LLVMTypeRef get_llvm_type(struct Context* ctx, struct Type* tt) {
  tt = TypeTable_get_base_type(tt);
  if(tt->kind == tk_BUILTIN) {

    // special case for 'void'
    if(TypeTable_equivalent_types(ctx, tt, TypeTable_get_type(ctx, "void"))) {
      return LLVMVoidTypeInContext(ctx->codegen->llvmContext);
    } else { // assumes ints
      return LLVMIntTypeInContext(ctx->codegen->llvmContext, tt->size * 8);
    }

  } else if(tt->kind == tk_POINTER) {
    return LLVMPointerTypeInContext(ctx->codegen->llvmContext, 0);
  } else if(tt->kind == tk_TYPEDEF) {
    int n_fields = TypeTable_get_num_fields(tt);
    LLVMTypeRef* fields = malloc(sizeof(*fields) * n_fields);
    int i = 0;
    LL_FOREACH(tt->fields, f) {
      fields[i] = get_llvm_type(ctx, f->type);
      i++;
    }
    LLVMTypeRef llvmTT =
        LLVMGetTypeByName2(ctx->codegen->llvmContext, tt->name);
    if(!llvmTT) {
      llvmTT = LLVMStructCreateNamed(ctx->codegen->llvmContext, tt->name);
      LLVMStructSetBody(llvmTT, fields, n_fields, 0);
    }
    return llvmTT;
  } else {
    UNIMPLEMENTED("unhandled\n");
  }
}

LLVMTypeRef get_llvm_type_ast(struct Context* ctx, struct AstNode* ast) {
  struct Type* tt = TypeTable_get_type_ast(ctx, ast);
  LLVMTypeRef llvmTT = get_llvm_type(ctx, tt);
  return llvmTT;
}

struct cg_value* allocate_stack_for_sym(
    struct Context* ctx,
    LLVMTypeRef cg_type,
    LLVMValueRef initial,
    struct ScopeSymbol* sym,
    struct Type* type) {
  // allocate a variable, make sure to switch the entry
  LLVMBasicBlockRef previousBB = LLVMGetInsertBlock(ctx->codegen->builder);
  LLVMBasicBlockRef entryBB =
      LLVMGetEntryBasicBlock(LLVMGetBasicBlockParent(previousBB));
  LLVMValueRef firstInst = LLVMGetFirstInstruction(entryBB);
  if(firstInst) LLVMPositionBuilderBefore(ctx->codegen->builder, firstInst);
  else LLVMPositionBuilderAtEnd(ctx->codegen->builder, entryBB);

  // get the alloca, add as a value
  LLVMValueRef stack_ptr = LLVMBuildAlloca(ctx->codegen->builder, cg_type, "");
  struct cg_value* val;
  if(sym) val = add_value(ctx, stack_ptr, cg_type, sym, type);
  else val = add_temp_value(ctx, stack_ptr, cg_type, type);

  // go back to bb
  LLVMPositionBuilderAtEnd(ctx->codegen->builder, previousBB);
  // set initial value
  LLVMBuildStore(ctx->codegen->builder, initial, stack_ptr);
  return val;
}
struct cg_value* allocate_stack_for_temp(
    struct Context* ctx,
    LLVMTypeRef cg_type,
    LLVMValueRef initial,
    struct Type* type) {
  return allocate_stack_for_sym(ctx, cg_type, initial, NULL, type);
}
