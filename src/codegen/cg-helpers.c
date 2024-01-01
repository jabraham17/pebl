

#include "cg-helpers.h"

#include "ast/Type.h"
#include "ast/ast.h"
#include "ast/scope-resolve.h"
#include "common/bsstring.h"

#include <string.h>

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
    struct ScopeSymbol* ss) {
  ASSERT(ss->sst == sst_Variable);
  struct cg_value* val =
      add_temp_value(ctx, value, cg_type, ss->ss_variable->type);
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

static LLVMValueRef build_ptrtoint_internal(
    struct Context* ctx,
    struct ScopeResult* scope,
    LLVMValueRef value,
    struct Type* intType,
    int is_const) {
  ASSERT(
      LLVMGetTypeKind(LLVMTypeOf(value)) == LLVMPointerTypeKind &&
      Type_is_integer(intType));
  int ptrSize = Type_ptr_size();
  // cast to int
  LLVMTypeRef intptrType =
      LLVMIntTypeInContext(ctx->codegen->llvmContext, ptrSize);
  LLVMValueRef llvmVal;
  if(!is_const)
    llvmVal =
        LLVMBuildPtrToInt(ctx->codegen->builder, value, intptrType, "cast");
  else llvmVal = LLVMConstPtrToInt(value, intptrType);

  LLVMTypeRef intLLVMType = get_llvm_type(ctx, scope, intType);
  // if the ptr size is not the size we are casting to, build int cast
  if(ptrSize != Type_get_size(intType)) {
    int sourceIsSigned = 0;
    if(!is_const)
      llvmVal = LLVMBuildIntCast2(
          ctx->codegen->builder,
          llvmVal,
          intLLVMType,
          sourceIsSigned,
          "cast");
    else llvmVal = LLVMConstIntCast(llvmVal, intLLVMType, sourceIsSigned);
  }
  ASSERT(LLVMGetTypeKind(LLVMTypeOf(llvmVal)) == LLVMGetTypeKind(intLLVMType));
  return llvmVal;
}

static struct cg_value* build_cast_internal(
    struct Context* ctx,
    struct ScopeResult* scope,
    struct Type* valueType,
    LLVMValueRef value,
    struct Type* newType,
    int is_const) {
  LLVMTypeRef newLLVMType = get_llvm_type(ctx, scope, newType);
  // check for noop cast
  if(Type_eq(valueType, newType)) {
    return add_temp_value(ctx, value, newLLVMType, newType);
  }

  // ptr -> ptr
  // also a noop cast in llvm terms, just adjust the Type
  if(Type_is_pointer(valueType) && Type_is_pointer(newType)) {
    return add_temp_value(ctx, value, newLLVMType, newType);
  }

  // int -> int
  // potentially a noop cast if the ints are the same size (and samed
  // signedness)
  if(Type_is_integer(valueType) && Type_is_integer(newType)) {
    if(Type_get_size(valueType) == Type_get_size(newType) &&
       Type_is_signed(valueType) == Type_is_signed(newType)) {
      // noop
      return add_temp_value(ctx, value, newLLVMType, newType);
    }
    int sourceIsSigned = Type_is_signed(valueType);
    LLVMValueRef llvmVal;
    if(!is_const)
      llvmVal = LLVMBuildIntCast2(
          ctx->codegen->builder,
          value,
          newLLVMType,
          sourceIsSigned,
          "cast");
    else llvmVal = LLVMConstIntCast(value, newLLVMType, sourceIsSigned);
    return add_temp_value(ctx, llvmVal, newLLVMType, newType);
  }

  // ptrtoint
  if(Type_is_pointer(valueType) && Type_is_integer(newType)) {
    LLVMValueRef llvmVal =
        build_ptrtoint_internal(ctx, scope, value, newType, is_const);
    return add_temp_value(ctx, llvmVal, newLLVMType, newType);
  }

  // inttoptr
  if(Type_is_integer(valueType) && Type_is_pointer(newType)) {
    // if the integer is not the same size as a ptr, build int cast
    int ptrSize = Type_ptr_size();
    LLVMValueRef llvmVal = value;
    if(ptrSize != Type_get_size(valueType)) {
      int sourceIsSigned = Type_is_signed(valueType);
      LLVMTypeRef intptrType =
          LLVMIntTypeInContext(ctx->codegen->llvmContext, ptrSize);
      if(!is_const)
        llvmVal = LLVMBuildIntCast2(
            ctx->codegen->builder,
            llvmVal,
            intptrType,
            sourceIsSigned,
            "cast");
      else llvmVal = LLVMConstIntCast(llvmVal, intptrType, sourceIsSigned);
    }
    // cast to ptr
    if(!is_const)
      llvmVal = LLVMBuildIntToPtr(
          ctx->codegen->builder,
          llvmVal,
          newLLVMType,
          "cast");
    else llvmVal = LLVMConstIntToPtr(llvmVal, newLLVMType);
    return add_temp_value(ctx, llvmVal, newLLVMType, newType);
  }

  // ptr -> bool
  // int -> bool
  // build ne compare aginst null value
  if((Type_is_integer(valueType) || Type_is_boolean(valueType)) &&
     Type_is_boolean(newType)) {
    LLVMValueRef nullValue = LLVMConstNull(LLVMTypeOf(value));
    LLVMValueRef llvmVal;
    if(!is_const)
      llvmVal = LLVMBuildICmp(
          ctx->codegen->builder,
          LLVMIntNE,
          value,
          nullValue,
          "cast");
    else llvmVal = LLVMConstICmp(LLVMIntNE, value, nullValue);
    return add_temp_value(ctx, llvmVal, newLLVMType, newType);
  }

  // bool -> int
  if(Type_is_boolean(valueType) && Type_is_integer(newType)) {
    // do extension
    int sourceIsSigned = Type_is_signed(valueType);
    LLVMValueRef llvmVal;
    if(!is_const)
      llvmVal = LLVMBuildIntCast2(
          ctx->codegen->builder,
          value,
          newLLVMType,
          sourceIsSigned,
          "cast");
    else llvmVal = LLVMConstIntCast(value, newLLVMType, sourceIsSigned);
    return add_temp_value(ctx, llvmVal, newLLVMType, newType);
  }

  // unimplemtned cast type
  return NULL;
}

LLVMValueRef build_ptrtoint(
    struct Context* ctx,
    struct ScopeResult* scope,
    LLVMValueRef value,
    struct Type* intType) {
  return build_ptrtoint_internal(ctx, scope, value, intType, 0);
}

struct cg_value* build_cast(
    struct Context* ctx,
    struct ScopeResult* scope,
    struct Type* valueType,
    LLVMValueRef value,
    struct Type* newType) {
  return build_cast_internal(ctx, scope, valueType, value, newType, 0);
}
struct cg_value* build_const_cast(
    struct Context* ctx,
    struct ScopeResult* scope,
    struct Type* valueType,
    LLVMValueRef value,
    struct Type* newType) {
  return build_cast_internal(ctx, scope, valueType, value, newType, 1);
}

struct cg_value* get_wide_string_literal(
    struct Context* ctx,
    struct ScopeResult* scope,
    wchar_t* str) {
  int len = wcslen(str) + 1;

  struct Type* charType = scope_get_Type_from_name(ctx, scope, "char", 1);

  LLVMTypeRef charTypeLLVM =
      LLVMIntTypeInContext(ctx->codegen->llvmContext, Type_get_size(charType));
  LLVMTypeRef strType = LLVMArrayType2(charTypeLLVM, len);
  LLVMValueRef strVal = LLVMAddGlobal(ctx->codegen->module, strType, "");
  LLVMValueRef* vals = malloc(sizeof(*vals) * len);
  for(int i = 0; i < len; i++) {
    vals[i] = LLVMConstInt(charTypeLLVM, str[i], 0);
  }
  LLVMSetInitializer(strVal, LLVMConstArray2(charTypeLLVM, vals, len));
  LLVMSetGlobalConstant(strVal, 1);
  LLVMSetLinkage(strVal, LLVMPrivateLinkage);
  LLVMSetUnnamedAddress(strVal, LLVMGlobalUnnamedAddr);

  return add_temp_value(
      ctx,
      strVal,
      LLVMPointerTypeInContext(ctx->codegen->llvmContext, 0),
      scope_get_Type_from_name(ctx, scope, "string", 1));
}

struct cg_value*
get_string_literal(struct Context* ctx, struct ScopeResult* scope, char* str) {
  int len = strlen(str) + 1;

  struct Type* charType = scope_get_Type_from_name(ctx, scope, "char", 1);

  LLVMTypeRef charTypeLLVM =
      LLVMIntTypeInContext(ctx->codegen->llvmContext, Type_get_size(charType));
  LLVMTypeRef strType = LLVMArrayType2(charTypeLLVM, len);
  LLVMValueRef strVal = LLVMAddGlobal(ctx->codegen->module, strType, "");
  LLVMValueRef* vals = malloc(sizeof(*vals) * len);
  for(int i = 0; i < len; i++) {
    vals[i] = LLVMConstInt(charTypeLLVM, str[i], 0);
  }
  LLVMSetInitializer(strVal, LLVMConstArray2(charTypeLLVM, vals, len));
  LLVMSetGlobalConstant(strVal, 1);
  LLVMSetLinkage(strVal, LLVMPrivateLinkage);
  LLVMSetUnnamedAddress(strVal, LLVMGlobalUnnamedAddr);

  return add_temp_value(
      ctx,
      strVal,
      LLVMPointerTypeInContext(ctx->codegen->llvmContext, 0),
      scope_get_Type_from_name(ctx, scope, "string", 1));
}

LLVMTypeRef
get_llvm_type(struct Context* ctx, struct ScopeResult* scope, struct Type* tt) {
  tt = Type_get_base_type(tt);
  if(tt->kind == tk_BUILTIN) {

    // special case for 'void'
    if(Type_is_void(tt)) {
      return LLVMVoidTypeInContext(ctx->codegen->llvmContext);
    } else { // assumes ints
      return LLVMIntTypeInContext(ctx->codegen->llvmContext, Type_get_size(tt));
    }

  } else if(tt->kind == tk_POINTER) {
    return LLVMPointerTypeInContext(ctx->codegen->llvmContext, 0);
  } else if(tt->kind == tk_TYPEDEF) {
    int n_fields = Type_get_num_fields(tt);
    LLVMTypeRef* fields = malloc(sizeof(*fields) * n_fields);
    LL_FOREACH_ENUMERATE(tt->fields, f, i) {
      fields[i] = get_llvm_type(ctx, scope, f->type);
    }
    LLVMTypeRef llvmTT =
        LLVMGetTypeByName2(ctx->codegen->llvmContext, tt->name);
    if(!llvmTT) {
      llvmTT = LLVMStructCreateNamed(ctx->codegen->llvmContext, tt->name);
      LLVMStructSetBody(llvmTT, fields, n_fields, 0);
    }
    return llvmTT;
  } else if(tt->kind == tk_OPAQUE) {
    ERROR(ctx, "unknown type definition for '%s', type is opaque\n", tt->name);
  } else {
    UNIMPLEMENTED("unhandled\n");
  }
}

LLVMTypeRef get_llvm_type_ast(
    struct Context* ctx,
    struct ScopeResult* scope,
    struct AstNode* ast) {
  struct Type* tt = scope_get_Type_from_ast(ctx, scope, ast, 1);
  LLVMTypeRef llvmTT = get_llvm_type(ctx, scope, tt);
  return llvmTT;
}
LLVMTypeRef get_llvm_type_sym(
    struct Context* ctx,
    struct ScopeResult* scope,
    struct ScopeSymbol* sym) {
  ASSERT(sym->sst == sst_Variable);
  struct Type* tt = sym->ss_variable->type;
  LLVMTypeRef llvmTT = get_llvm_type(ctx, scope, tt);
  return llvmTT;
}

static struct cg_value* allocate_stack_helper(
    struct Context* ctx,
    LLVMTypeRef cg_type,
    LLVMValueRef initial,
    struct ScopeSymbol* sym,
    struct Type* type) {
  // sym can be NULL, or it must be a variable
  ASSERT(!sym || sym->sst == sst_Variable);
  // allocate a variable, make sure to switch the entry
  LLVMBasicBlockRef previousBB = LLVMGetInsertBlock(ctx->codegen->builder);
  LLVMBasicBlockRef entryBB =
      LLVMGetEntryBasicBlock(LLVMGetBasicBlockParent(previousBB));
  LLVMValueRef firstInst = LLVMGetFirstInstruction(entryBB);
  if(firstInst) LLVMPositionBuilderBefore(ctx->codegen->builder, firstInst);
  else LLVMPositionBuilderAtEnd(ctx->codegen->builder, entryBB);

  char* name =
      sym ? ast_Identifier_name(ast_Variable_name(sym->ss_variable->variable))
          : "temp";

  // get the alloca, add as a value
  LLVMValueRef stack_ptr =
      LLVMBuildAlloca(ctx->codegen->builder, cg_type, name);
  struct cg_value* val;
  if(sym) val = add_value(ctx, stack_ptr, cg_type, sym);
  else val = add_temp_value(ctx, stack_ptr, cg_type, type);

  // go back to bb
  LLVMPositionBuilderAtEnd(ctx->codegen->builder, previousBB);
  // set initial value
  LLVMBuildStore(ctx->codegen->builder, initial, stack_ptr);
  return val;
}

struct cg_value* allocate_stack_for_sym(
    struct Context* ctx,
    LLVMTypeRef cg_type,
    LLVMValueRef initial,
    struct ScopeSymbol* sym) {
  ASSERT(sym->sst == sst_Variable);
  return allocate_stack_helper(
      ctx,
      cg_type,
      initial,
      sym,
      sym->ss_variable->type);
}
struct cg_value* allocate_stack_for_temp(
    struct Context* ctx,
    LLVMTypeRef cg_type,
    LLVMValueRef initial,
    struct Type* type) {
  return allocate_stack_helper(ctx, cg_type, initial, NULL, type);
}
