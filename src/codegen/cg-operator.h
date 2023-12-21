#ifndef CG_OPERATOR_H_
#define CG_OPERATOR_H_

#include "ast/ast.h"
#include "codegen/codegen-llvm.h"

struct cg_value* codegenBinaryOperator(
    struct Context* context,
    enum OperatorType op,
    struct cg_value* lhs,
    struct cg_value* rhs);
struct cg_value* codegenUnaryOperator(
    struct Context* context,
    enum OperatorType op,
    struct cg_value* operand);

#endif
