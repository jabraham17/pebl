
#include "cg-operator.h"

#include "ast/Type.h"
#include "ast/scope-resolve.h"

#include "cg-helpers.h"
#include "cg-inst.h"

static struct cg_value* codegenOperator_sintBOp(
    struct Context* ctx,
    __attribute__((unused)) struct ScopeResult* scope,
    enum OperatorType op,
    struct AstNode* lhsAst,
    struct AstNode* rhsAst,
    struct Type* resType) {
  struct cg_value* lhs = codegen_helper(ctx, lhsAst, scope);
  struct cg_value* rhs = codegen_helper(ctx, rhsAst, scope);
  ASSERT(
      Type_is_signed(lhs->type) && Type_is_signed(rhs->type) &&
      Type_is_signed(resType));

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

  LLVMTypeRef resLLVMType = get_llvm_type(ctx, scope, resType);
  if(LLVMGetTypeKind(resLLVMType) != LLVMGetTypeKind(LLVMTypeOf(resVal))) {
    resVal =
        LLVMBuildIntCast2(ctx->codegen->builder, resVal, resLLVMType, 1, "");
  }

  struct cg_value* res =
      allocate_stack_for_temp(ctx, resLLVMType, resVal, resType);
  return res;
}

static struct cg_value* codegenOperator_compare(
    struct Context* ctx,
    struct ScopeResult* scope,
    enum OperatorType op,
    struct AstNode* lhsAst,
    struct AstNode* rhsAst,
    struct Type* resType) {
  struct cg_value* lhs = codegen_helper(ctx, lhsAst, scope);
  struct cg_value* rhs = codegen_helper(ctx, rhsAst, scope);

  LLVMValueRef lhsVal =
      LLVMBuildLoad2(ctx->codegen->builder, lhs->cg_type, lhs->value, "");
  LLVMValueRef rhsVal =
      LLVMBuildLoad2(ctx->codegen->builder, rhs->cg_type, rhs->value, "");

  // if they are both ptrs, no need to change anything.
  // if one or the other is a ptr, need to do 'ptrtoint'
  if(LLVMGetTypeKind(LLVMTypeOf(lhsVal)) != LLVMPointerTypeKind ||
     LLVMGetTypeKind(LLVMTypeOf(rhsVal)) != LLVMPointerTypeKind) {
    if(LLVMGetTypeKind(LLVMTypeOf(lhsVal)) == LLVMPointerTypeKind) {
      lhsVal = build_ptrtoint(
          ctx,
          scope,
          lhsVal,
          Type_int_type(ctx, Type_ptr_size()));
    }
    if(LLVMGetTypeKind(LLVMTypeOf(rhsVal)) == LLVMPointerTypeKind) {
      rhsVal = build_ptrtoint(
          ctx,
          scope,
          rhsVal,
          Type_int_type(ctx, Type_ptr_size()));
    }
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
  LLVMTypeRef resLLVMType = get_llvm_type(ctx, scope, resType);
  ASSERT(LLVMGetTypeKind(LLVMTypeOf(resVal)) == LLVMGetTypeKind(resLLVMType));

  struct cg_value* res =
      allocate_stack_for_temp(ctx, resLLVMType, resVal, resType);
  return res;
}

static struct cg_value* codegenOperator_booleanAnd(
    struct Context* ctx,
    struct ScopeResult* scope,
    __attribute__((unused)) enum OperatorType op,
    struct AstNode* lhsAst,
    struct AstNode* rhsAst,
    struct Type* resType) {

  ASSERT(Type_is_boolean(resType));
  LLVMTypeRef resLLVMType = get_llvm_type(ctx, scope, resType);

  struct cg_value* res = allocate_stack_for_temp(
      ctx,
      resLLVMType,
      LLVMConstInt(resLLVMType, 0, 0),
      resType);

  // this works by creating two branches. We always eval the lhs. if its true,
  // we have to eval the rhs. If the lhs is false, no need to eval the rhs
  // (shortcircuit) this is easy to do with branching

  LLVMBasicBlockRef currBB = LLVMGetInsertBlock(ctx->codegen->builder);
  LLVMValueRef currentFunc = LLVMGetBasicBlockParent(currBB);
  // create BBs
  LLVMBasicBlockRef rhsBB =
      LLVMCreateBasicBlockInContext(ctx->codegen->llvmContext, "and.rhs");
  LLVMBasicBlockRef endBB =
      LLVMCreateBasicBlockInContext(ctx->codegen->llvmContext, "and.end");

  struct cg_value* lhs = codegen_helper(ctx, lhsAst, scope);
  LLVMValueRef lhsVal =
      LLVMBuildLoad2(ctx->codegen->builder, lhs->cg_type, lhs->value, "");
  LLVMValueRef lhsCond = LLVMBuildICmp(
      ctx->codegen->builder,
      LLVMIntNE,
      lhsVal,
      LLVMConstNull(LLVMTypeOf(lhsVal)),
      "");
  LLVMBuildCondBr(ctx->codegen->builder, lhsCond, rhsBB, endBB);

  LLVMAppendExistingBasicBlock(currentFunc, rhsBB);
  LLVMPositionBuilderAtEnd(ctx->codegen->builder, rhsBB);

  struct cg_value* rhs = codegen_helper(ctx, rhsAst, scope);
  LLVMValueRef rhsVal =
      LLVMBuildLoad2(ctx->codegen->builder, rhs->cg_type, rhs->value, "");
  LLVMValueRef rhsCond = LLVMBuildICmp(
      ctx->codegen->builder,
      LLVMIntNE,
      rhsVal,
      LLVMConstNull(LLVMTypeOf(rhsVal)),
      "");
  ASSERT(LLVMGetTypeKind(resLLVMType) == LLVMGetTypeKind(LLVMTypeOf(rhsCond)));
  LLVMBuildStore(ctx->codegen->builder, rhsCond, res->value);
  LLVMBuildBr(ctx->codegen->builder, endBB);

  // move to end and keep going
  LLVMAppendExistingBasicBlock(currentFunc, endBB);
  LLVMPositionBuilderAtEnd(ctx->codegen->builder, endBB);

  return res;
}

static struct cg_value* codegenOperator_negate(
    struct Context* ctx,
    __attribute__((unused)) struct ScopeResult* scope,
    __attribute__((unused)) enum OperatorType op,
    struct AstNode* operandAst,
    struct Type* resType) {
  struct cg_value* operand = codegen_helper(ctx, operandAst, scope);

  LLVMValueRef operandVal = LLVMBuildLoad2(
      ctx->codegen->builder,
      operand->cg_type,
      operand->value,
      "");
  LLVMValueRef nullVal = LLVMConstNull(LLVMTypeOf(operandVal));
  // !1 -> 1 == 0 -> 0
  // !0 -> 0 == 0 -> 1
  LLVMValueRef resVal =
      LLVMBuildICmp(ctx->codegen->builder, LLVMIntEQ, operandVal, nullVal, "");
  LLVMTypeRef resLLVMType = get_llvm_type(ctx, scope, resType);
  ASSERT(LLVMGetTypeKind(LLVMTypeOf(resVal)) == LLVMGetTypeKind(resLLVMType));

  struct cg_value* res =
      allocate_stack_for_temp(ctx, resLLVMType, resVal, resType);
  return res;
}

static struct cg_value* codegenOperator_cast(
    struct Context* ctx,
    struct ScopeResult* scope,
    __attribute__((unused)) enum OperatorType op,
    struct AstNode* lhsAst,
    struct AstNode* rhsAst,
    __attribute__((unused)) struct Type* resType) {

  struct cg_value* lhsVal = codegen_helper(ctx, lhsAst, scope);
  struct Type* rhsType = scope_get_Type_from_ast(ctx, scope, rhsAst, 1);
  struct Type* lhsType = lhsVal->type;
  LLVMValueRef val =
      LLVMBuildLoad2(ctx->codegen->builder, lhsVal->cg_type, lhsVal->value, "");
  struct cg_value* casted = build_cast(ctx, scope, lhsType, val, rhsType);
  // if not casted, unknown cast
  if(casted) {
    return allocate_stack_for_temp(
        ctx,
        casted->cg_type,
        casted->value,
        casted->type);
  } else {
    return NULL;
  }
}

static struct cg_value* codegenOperator_addrOffset(
    struct Context* ctx,
    struct ScopeResult* scope,
    enum OperatorType op,
    struct AstNode* lhsAst,
    struct AstNode* rhsAst,
    __attribute__((unused)) struct Type* resType) {

  struct cg_value* lhs = codegen_helper(ctx, lhsAst, scope);
  struct cg_value* rhs = codegen_helper(ctx, rhsAst, scope);

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
    struct AstNode* operandAst,
    __attribute__((unused)) struct Type* resType) {

  struct cg_value* operand = codegen_helper(ctx, operandAst, scope);

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
    struct AstNode* operandAst,
    __attribute__((unused)) struct Type* resType) {
  // just return the stack ptr for this
  struct cg_value* operand = codegen_helper(ctx, operandAst, scope);
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
    struct Type* type,
    char* typename) {
  // any type
  if(typename == NULL) return 1;
  if(typename[0] == '\0') return 1;

  // type is any pointer
  if(typename[0] == '*' && typename[1] == '\0') {
    return Type_is_pointer(Type_get_base_type(type));
  }

  struct Type* t = scope_get_Type_from_name(ctx, scope, typename, 1);

  return Type_eq(type, t);
}

// convert the types specified in .def into a Type
static struct Type* get_type_from_def_type(
    struct Context* ctx,
    struct ScopeResult* scope,
    char* type) {
  // any type, return NULL
  if(type == NULL) return NULL;
  if(type[0] == '\0') return NULL;

  // type is void*
  if(type[0] == '*' && type[1] == '\0') {
    return Type_get_ptr_type(scope_get_Type_from_name(ctx, scope, "void", 1));
  }
  return scope_get_Type_from_name(ctx, scope, type, 1);
}

struct cg_value* codegenBinaryOperator(
    struct Context* ctx,
    struct ScopeResult* scope,
    struct AstNode* expr) {
  enum OperatorType op = ast_Expr_op(expr);
  struct AstNode* lhs = ast_Expr_lhs(expr);
  struct AstNode* rhs = ast_Expr_rhs(expr);
  struct Type* lhsAstType = scope_get_Type_from_ast(ctx, scope, lhs, 1);
  struct Type* rhsAstType = scope_get_Type_from_ast(ctx, scope, rhs, 1);

#define BINARY_EXPR(opType, lhsType, rhsType, resType, funcName)               \
  if(op == op_##opType && typesMatch(ctx, scope, lhsAstType, lhsType) &&       \
     typesMatch(ctx, scope, rhsAstType, rhsType)) {                            \
    return codegenOperator_##funcName(                                         \
        ctx,                                                                   \
        scope,                                                                 \
        op,                                                                    \
        lhs,                                                                   \
        rhs,                                                                   \
        get_type_from_def_type(ctx, scope, resType));                          \
  }
#include "definitions/expression-types.def"

  return NULL;
}

struct cg_value* codegenUnaryOperator(
    struct Context* ctx,
    struct ScopeResult* scope,
    struct AstNode* expr) {
  enum OperatorType op = ast_Expr_op(expr);
  struct AstNode* operand = ast_Expr_lhs(expr);
  struct Type* operandAstType = scope_get_Type_from_ast(ctx, scope, operand, 1);

#define UNARY_EXPR(opType, operandType, resType, funcName)                     \
  if(op == op_##opType &&                                                      \
     typesMatch(ctx, scope, operandAstType, operandType)) {                    \
    return codegenOperator_##funcName(                                         \
        ctx,                                                                   \
        scope,                                                                 \
        op,                                                                    \
        operand,                                                               \
        get_type_from_def_type(ctx, scope, resType));                          \
  }
#include "definitions/expression-types.def"

  return NULL;
}
