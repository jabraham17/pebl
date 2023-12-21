#ifndef PEBL_AST_H_
#define PEBL_AST_H_

#include "common/ll-common.h"
#include "context/context.h"

#include <stdlib.h>
enum OperatorType {
  op_NONE = 0,
  op_PLUS,
  op_MINUS,
  op_MULT,
  op_DIVIDE,
  op_AND,
  op_OR,
  op_LT,
  op_GT,
  op_LTEQ,
  op_GTEQ,
  op_EQ,
  op_NEQ,
  op_NOT,
  op_CAST,
  op_TAKE_ADDRESS,
  op_PTR_DEREFERENCE,

};
void OperatorType_tostring(char* buf, enum OperatorType op);
enum AstType {
  ast_Identifier,
  ast_Typename,
  ast_FieldAccess,
  ast_Type,
  ast_Variable,
  ast_Function,
  ast_Assignment,
  ast_Return,
  ast_Break,
  ast_Block,
  ast_Conditional,
  ast_While,
  ast_Expr,
  ast_Call,
  ast_Number,
  ast_String,
};
#define AST_MAX_CHILDREN 4

struct AstNode {
  enum AstType at;
  struct AstNode* next;
  struct AstNode* children[AST_MAX_CHILDREN];
  int int_value;
  char* str_value;
};

struct AstNode* ast_allocate(enum AstType at);
struct AstNode* ast_next(struct AstNode* ast);
void ast_append(struct AstNode** head, struct AstNode* tail);
enum AstType ast_type(struct AstNode* ast);
int ast_is_type(struct AstNode* ast, enum AstType at);
int ast_num_children(struct AstNode* ast);

void dump_ast(struct Context* context);
void verify_ast(struct Context* context);

#define ast_foreach(root, name) LL_FOREACH(root, name)
#define ast_foreach_child(root, name)                                          \
  struct AstNode* name = NULL;                                                 \
  for(int i = 0;                                                               \
      ((name = (root)->children[i]), 1) && i < ast_num_children(root);         \
      i++)

/* Identifier */
struct AstNode* ast_build_Identifier(char* name);
int ast_verify_Identifier(struct AstNode* ast);
char* ast_Identifier_name(struct AstNode* ast);

/* Typename */
struct AstNode* ast_build_Typename(char* name);
struct AstNode* ast_build_Typename2(char* name, int ptr_level);
int ast_verify_Typename(struct AstNode* ast);
char* ast_Typename_name(struct AstNode* ast);
int ast_Typename_ptr_level(struct AstNode* ast);

/* FieldAccess */
struct AstNode* ast_build_FieldAccess(
    struct AstNode* object,
    int object_is_ptr,
    struct AstNode* field);
int ast_verify_FieldAccess(struct AstNode* ast);
struct AstNode*
ast_FieldAccess_object(struct AstNode* ast); // returns an Identifier
struct AstNode*
ast_FieldAccess_field(struct AstNode* ast); // returns an Identifier
int ast_FieldAccess_object_is_ptr(struct AstNode* ast);

/* Type */
struct AstNode* ast_build_Type(struct AstNode* name, struct AstNode* args);
struct AstNode*
ast_build_TypeAlias(struct AstNode* name, struct AstNode* alias);
int ast_verify_Type(struct AstNode* ast);
struct AstNode* ast_Type_name(struct AstNode* ast);  // returns a Typename
struct AstNode* ast_Type_args(struct AstNode* ast);  // returns a Variable
struct AstNode* ast_Type_alias(struct AstNode* ast); // returns a Typename
int ast_Type_is_alias(struct AstNode* ast);

/* Variable */
struct AstNode* ast_build_Variable(
    struct AstNode* name,
    struct AstNode* type,

    struct AstNode* expr);
int ast_verify_Variable(struct AstNode* ast);
struct AstNode* ast_Variable_name(struct AstNode* ast); // returns an Identifier
struct AstNode* ast_Variable_type(struct AstNode* ast); // returns a Typename
struct AstNode* ast_Variable_expr(struct AstNode* ast); // returns an Expr

/* Function */
struct AstNode* ast_build_FunctionHeader(
    struct AstNode* name,
    struct AstNode* args,
    struct AstNode* ret_type);
struct AstNode* ast_build_ExternFunction(struct AstNode* header);
struct AstNode* ast_build_ExportFunction(struct AstNode* header);
struct AstNode*
ast_build_Function(struct AstNode* header, struct AstNode* body);
int ast_verify_Function(struct AstNode* ast);
struct AstNode* ast_Function_name(struct AstNode* ast); // returns an Identifier
struct AstNode* ast_Function_args(struct AstNode* ast); // returns a Variable
int ast_Function_num_args(struct AstNode* ast);
struct AstNode*
ast_Function_ret_type(struct AstNode* ast); // returns a Typename
struct AstNode* ast_Function_body(struct AstNode* ast);
int ast_Function_has_body(struct AstNode* ast);
int ast_Function_is_extern(struct AstNode* ast);
int ast_Function_is_export(struct AstNode* ast);

/* Assignment */
struct AstNode* ast_build_Assignment(
    struct AstNode* lhs,
    struct AstNode* expr,
    int is_ptr_access);
int ast_verify_Assignment(struct AstNode* ast);
struct AstNode* ast_Assignment_lhs(
    struct AstNode* ast); // returns an Identifier or a FieldAccess
int ast_Assignment_is_ptr_access(struct AstNode* ast);
struct AstNode* ast_Assignment_expr(struct AstNode* ast); // returns an Expr

/* Return */
struct AstNode* ast_build_Return(struct AstNode* expr);
int ast_verify_Return(struct AstNode* expr);
struct AstNode* ast_Return_expr(struct AstNode* ast); // returns an Expr

/* Break */
struct AstNode* ast_build_Break();
int ast_verify_Break(struct AstNode* ast);
// no methods

/* Block */
struct AstNode* ast_build_Block(struct AstNode* stmts);
struct AstNode* ast_build_EmptyBlock();
int ast_verify_Block(struct AstNode* ast);
struct AstNode* ast_Block_stmts(struct AstNode* ast);
int ast_Block_is_empty(struct AstNode* ast);

/* Conditional */
struct AstNode* ast_build_Conditional(
    struct AstNode* condition,
    struct AstNode* if_body,
    struct AstNode* else_body);
int ast_verify_Conditional(struct AstNode* ast);
struct AstNode*
ast_Conditional_condition(struct AstNode* ast); // returns an Expr
struct AstNode* ast_Conditional_if_body(struct AstNode* ast);
struct AstNode* ast_Conditional_else_body(struct AstNode* ast);

/* While */
struct AstNode*
ast_build_While(struct AstNode* condition, struct AstNode* body);
int ast_verify_While(struct AstNode* ast);
struct AstNode* ast_While_condition(struct AstNode* ast); // returns an Expr
struct AstNode* ast_While_body(struct AstNode* ast);

/* Expr */
struct AstNode* ast_build_Expr_plain(struct AstNode* lhs);
struct AstNode* ast_build_Expr_binop(
    struct AstNode* lhs,
    struct AstNode* rhs,
    enum OperatorType op);
struct AstNode* ast_build_Expr_uop(struct AstNode* lhs, enum OperatorType op);
int ast_verify_Expr(struct AstNode* ast);
struct AstNode* ast_Expr_lhs(struct AstNode* ast);
struct AstNode* ast_Expr_rhs(struct AstNode* ast);
enum OperatorType ast_Expr_op(struct AstNode* ast);
int ast_Expr_is_plain(struct AstNode* ast);
int ast_Expr_is_binop(struct AstNode* ast);
int ast_Expr_is_uop(struct AstNode* ast);

/* Call */
struct AstNode* ast_build_Call(struct AstNode* name, struct AstNode* args);
int ast_verify_Call(struct AstNode* ast);
struct AstNode* ast_Call_name(struct AstNode* ast); // returns an Identifier
struct AstNode* ast_Call_args(struct AstNode* ast); // returns an Identifier
int ast_Call_num_args(struct AstNode* ast);

/* Number */
struct AstNode* ast_build_Number(int value);
int ast_verify_Number(struct AstNode* ast);
int ast_Number_value(struct AstNode* ast);

/* String */
struct AstNode* ast_build_String(char* value);
int ast_verify_String(struct AstNode*);
char* ast_String_value(struct AstNode* ast);

#endif
