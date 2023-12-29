#ifndef CG_HELPER_H_
#define CG_HELPER_H_

#include "codegen/codegen-llvm.h"

struct cg_value* get_value(struct Context* ctx, struct ScopeSymbol* ss);
struct cg_value* add_temp_value(
    struct Context* ctx,
    LLVMValueRef value,
    LLVMTypeRef cg_type,
    struct Type* type);
struct cg_value* add_value(
    struct Context* ctx,
    LLVMValueRef value,
    LLVMTypeRef cg_type,
    struct ScopeSymbol* ss);

struct cg_function* get_function_named(struct Context* ctx, char* name);
struct cg_function* add_function(
    struct Context* ctx,
    char* name,
    char* mname,
    LLVMValueRef func,
    LLVMTypeRef cg_type,
    struct Type* rettype);

char* mangled_name(struct Context* ctx, struct AstNode* ast);

// always works on plain values, NOT stack ptrs
// proper usage needs to load a value (if not one already) before calling these
LLVMValueRef build_ptrtoint(struct Context* ctx, struct ScopeResult* scope, LLVMValueRef value, struct Type* intType);
struct cg_value* build_cast(
    struct Context* ctx,
    struct ScopeResult* scope,
    struct Type* valueType,
    LLVMValueRef value,
    struct Type* newType);
struct cg_value* build_const_cast(
    struct Context* ctx,
    struct ScopeResult* scope,
    struct Type* valueType,
     LLVMValueRef value,
    struct Type* newType);

struct cg_value*
get_string_literal(struct Context* ctx, struct ScopeResult* scope, char* str);

LLVMTypeRef
get_llvm_type(struct Context* ctx, struct ScopeResult* scope, struct Type* tt);
LLVMTypeRef get_llvm_type_ast(
    struct Context* ctx,
    struct ScopeResult* scope,
    struct AstNode* ast);
LLVMTypeRef get_llvm_type_sym(
    struct Context* ctx,
    struct ScopeResult* scope,
    struct ScopeSymbol* sym);

struct cg_value* allocate_stack_for_sym(
    struct Context* ctx,
    LLVMTypeRef cg_type,
    LLVMValueRef initial,
    struct ScopeSymbol* sym);
struct cg_value* allocate_stack_for_temp(
    struct Context* ctx,
    LLVMTypeRef cg_type,
    LLVMValueRef initial,
    struct Type* type);

#endif
