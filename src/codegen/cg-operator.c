
#include "cg-operator.h"

#include "ast/Type.h"
#include "ast/scope-resolve.h"

#include "cg-helpers.h"

static struct cg_value* codegenOperator_intBOp(
    struct Context* ctx,
    __attribute__((unused)) struct ScopeResult* scope,
    enum OperatorType op,
    struct cg_value* lhs,
    struct cg_value* rhs) {

  LLVMValueRef lhsVal =
      LLVMBuildLoad2(ctx->codegen->builder, lhs->cg_type, lhs->value, "");
  LLVMValueRef rhsVal =
      LLVMBuildLoad2(ctx->codegen->builder, rhs->cg_type, rhs->value, "");

  LLVMValueRef resVal;
  if(op == op_PLUS) {
    resVal = LLVMBuildAdd(ctx->codegen->builder, lhsVal, rhsVal, "");
  } else if(op == op_MINUS) {
    resVal = LLVMBuildSub(ctx->codegen->builder, lhsVal, rhsVal, "");
  } else if(op == op_MULT) {
    resVal = LLVMBuildMul(ctx->codegen->builder, lhsVal, rhsVal, "");
  } else if(op == op_DIVIDE) {
    resVal = LLVMBuildSDiv(ctx->codegen->builder, lhsVal, rhsVal, "");
  } else if(op == op_DIVIDE) {
    resVal = LLVMBuildSDiv(ctx->codegen->builder, lhsVal, rhsVal, "");
  } else if(op == op_AND) {
    resVal = LLVMBuildAnd(ctx->codegen->builder, lhsVal, rhsVal, "");
  } else if(op == op_OR) {
    resVal = LLVMBuildAnd(ctx->codegen->builder, lhsVal, rhsVal, "");
  } else {
    ERROR(ctx, "could not codegen operator\n");
  }

  struct cg_value* res =
      allocate_stack_for_temp(ctx, LLVMTypeOf(resVal), resVal, lhs->type);
  return res;
}

static struct cg_value* codegenOperator_intCompare(
    struct Context* ctx,
    struct ScopeResult* scope,
    enum OperatorType op,
    struct cg_value* lhs,
    struct cg_value* rhs) {
    ASSERT(Type_eq(lhs->type, rhs->type));

  LLVMValueRef lhsVal =
      LLVMBuildLoad2(ctx->codegen->builder, lhs->cg_type, lhs->value, "");
  LLVMValueRef rhsVal =
      LLVMBuildLoad2(ctx->codegen->builder, rhs->cg_type, rhs->value, "");

  LLVMIntPredicate pred;
  if(op == op_LT) pred = LLVMIntSLT;
  else if(op == op_GT) pred = LLVMIntSGT;
  else if(op == op_LTEQ) pred = LLVMIntSLE;
  else if(op == op_GTEQ) pred = LLVMIntSGE;
  else if(op == op_EQ) pred = LLVMIntEQ;
  else if(op == op_NEQ) pred = LLVMIntNE;
  else {
    ERROR(ctx, "could not codegen operator\n");
  }
  LLVMValueRef resVal =
      LLVMBuildICmp(ctx->codegen->builder, pred, lhsVal, rhsVal, "");
  struct Type* resType = lhs->type
  LLVMTypeRef resLLVMType = get_llvm_type(ctx, scope, resType);
  resVal = LLVMBuildIntCast2(ctx->codegen->builder, resVal, resLLVMType, 1, "");

  struct cg_value* res =
      allocate_stack_for_temp(ctx, resLLVMType, resVal, resType);
  return res;
}

static struct cg_value* codegenOperator_ptrCompare(
    struct Context* ctx,
    struct ScopeResult* scope,
    enum OperatorType op,
    struct cg_value* lhs,
    struct cg_value* rhs) {

  LLVMValueRef lhsVal =
      LLVMBuildLoad2(ctx->codegen->builder, lhs->cg_type, lhs->value, "");
  LLVMValueRef rhsVal =
      LLVMBuildLoad2(ctx->codegen->builder, rhs->cg_type, rhs->value, "");

  if(LLVMGetTypeKind(LLVMTypeOf(lhsVal)) == LLVMPointerTypeKind) {
    lhsVal = LLVMBuildPtrToInt(
        ctx->codegen->builder,
        lhsVal,
        LLVMInt64TypeInContext(ctx->codegen->llvmContext),
        "");
  }
  if(LLVMGetTypeKind(LLVMTypeOf(rhsVal)) == LLVMPointerTypeKind) {
    rhsVal = LLVMBuildPtrToInt(
        ctx->codegen->builder,
        rhsVal,
        LLVMInt64TypeInContext(ctx->codegen->llvmContext),
        "");
  }

  LLVMIntPredicate pred;
  if(op == op_LT) pred = LLVMIntSLT;
  else if(op == op_GT) pred = LLVMIntSGT;
  else if(op == op_LTEQ) pred = LLVMIntSLE;
  else if(op == op_GTEQ) pred = LLVMIntSGE;
  else if(op == op_EQ) pred = LLVMIntEQ;
  else if(op == op_NEQ) pred = LLVMIntNE;
  else {
    ERROR(ctx, "could not codegen operator\n");
  }
  LLVMValueRef resVal =
      LLVMBuildICmp(ctx->codegen->builder, pred, lhsVal, rhsVal, "");
  struct Type* resType = scope_get_Type_from_name(ctx, scope, "int", 1);
  LLVMTypeRef resLLVMType = get_llvm_type(ctx, scope, resType);
  resVal = LLVMBuildIntCast2(ctx->codegen->builder, resVal, resLLVMType, 1, "");

  struct cg_value* res =
      allocate_stack_for_temp(ctx, resLLVMType, resVal, resType);
  return res;
}

static struct cg_value* codegenOperator_negate(
    struct Context* ctx,
    __attribute__((unused)) struct ScopeResult* scope,
    __attribute__((unused)) enum OperatorType op,
    struct cg_value* operand) {

  LLVMValueRef operandVal = LLVMBuildLoad2(
      ctx->codegen->builder,
      operand->cg_type,
      operand->value,
      "");
  LLVMValueRef nullVal = LLVMConstNull(LLVMTypeOf(operandVal));
  // !1 -> 1 == 0 -> 0
  // !0 -> 0 == 0 -> 1
  LLVMValueRef compare =
      LLVMBuildICmp(ctx->codegen->builder, LLVMIntEQ, operandVal, nullVal, "");

  struct cg_value* res = allocate_stack_for_temp(
      ctx,
      LLVMTypeOf(compare),
      compare,
      NULL /**TODO:this is probably wrong*/);
  return res;
}

static struct cg_value* codegenOperator_addrOffset(
    struct Context* ctx,
    struct ScopeResult* scope,
    enum OperatorType op,
    struct cg_value* lhs,
    struct cg_value* rhs) {

  struct cg_value* ptr;
  struct cg_value* offset;
  if(Type_is_pointer(Type_get_base_type(lhs->type))) {
    ptr = lhs;
    offset = rhs;
  } else {
    ptr = rhs;
    offset = lhs;
  }
  struct Type* ptrType = Type_get_base_type(ptr->type);
  ASSERT(Type_is_pointer(ptrType));

  if(op != op_PLUS) {
    UNIMPLEMENTED("unimplemented op\n");
  }

  LLVMValueRef ptrVal =
      LLVMBuildLoad2(ctx->codegen->builder, ptr->cg_type, ptr->value, "");
  LLVMValueRef offsetVal =
      LLVMBuildLoad2(ctx->codegen->builder, offset->cg_type, offset->value, "");

  LLVMTypeRef gepType = get_llvm_type(ctx, scope, ptrType->pointer_to);
  LLVMValueRef* gepIdx = &offsetVal;
  LLVMValueRef resVal =
      LLVMBuildGEP2(ctx->codegen->builder, gepType, ptrVal, gepIdx, 1, "");

  struct cg_value* res =
      allocate_stack_for_temp(ctx, LLVMTypeOf(resVal), resVal, ptrType);
  return res;
}

static struct cg_value* codegenOperator_getValueAtAddress(
    struct Context* ctx,
    struct ScopeResult* scope,
    __attribute__((unused)) enum OperatorType op,
    struct cg_value* operand) {
  struct Type* ptrType = Type_get_base_type(operand->type);
  ASSERT(Type_is_pointer(ptrType));
  LLVMValueRef ptr = LLVMBuildLoad2(
      ctx->codegen->builder,
      operand->cg_type,
      operand->value,
      "");
  // value should be a pointer, load it, then its pointer

  ASSERT(LLVMGetTypeKind(LLVMTypeOf(ptr)) == LLVMPointerTypeKind);
  LLVMTypeRef pointerToLLVMType =
      get_llvm_type(ctx, scope, ptrType->pointer_to);
  LLVMValueRef loaded =
      LLVMBuildLoad2(ctx->codegen->builder, pointerToLLVMType, ptr, "");
  struct cg_value* derefed_val = allocate_stack_for_temp(
      ctx,
      pointerToLLVMType,
      loaded,
      ptrType->pointer_to);
  return derefed_val;
}

static struct cg_value* codegenOperator_getAddressOfValue(
    struct Context* ctx,
    __attribute__((unused)) struct ScopeResult* scope,
    __attribute__((unused)) enum OperatorType op,
    struct cg_value* operand) {
  // just return the stack ptr for this
  struct Type* operandTypePtr = Type_get_ptr_type(operand->type);
  ASSERT(LLVMGetTypeKind(LLVMTypeOf(operand->value)) == LLVMPointerTypeKind);
  struct cg_value* address = allocate_stack_for_temp(
      ctx,
      LLVMTypeOf(operand->value),
      operand->value,
      operandTypePtr);
  return address;
}

static int typesMatch(
    struct Context* ctx,
    struct ScopeResult* scope,
    struct cg_value* val,
    char* type) {
  // any type
  if(type == NULL) return 1;
  if(type[0] == '\0') return 1;

  // type is any pointer
  if(type[0] == '*' && type[1] == '\0') {
    return Type_is_pointer(Type_get_base_type(val->type));
  }

  struct Type* t = scope_get_Type_from_name(ctx, scope, type, 1);

  return Type_eq(val->type, t);
}

struct cg_value* codegenBinaryOperator(
    struct Context* ctx,
    struct ScopeResult* scope,
    enum OperatorType op,
    struct cg_value* lhs,
    struct cg_value* rhs) {

#define CODEGEN_BINARY_OPERATOR(opType, lhsType, rhsType, funcName)            \
  if(op == op_##opType && typesMatch(ctx, scope, lhs, lhsType) &&              \
     typesMatch(ctx, scope, rhs, rhsType)) {                                   \
    return codegenOperator_##funcName(ctx, scope, op, lhs, rhs);               \
  }
#include "cg-operator.def"

  return NULL;
}

struct cg_value* codegenUnaryOperator(
    struct Context* ctx,
    struct ScopeResult* scope,
    enum OperatorType op,
    struct cg_value* operand) {

#define CODEGEN_UNARY_OPERATOR(opType, operandType, funcName)                  \
  if(op == op_##opType && typesMatch(ctx, scope, operand, operandType)) {      \
    return codegenOperator_##funcName(ctx, scope, op, operand);                \
  }
#include "cg-operator.def"

  return NULL;
}
