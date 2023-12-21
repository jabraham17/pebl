#include "ast/ast.h"

#include "common/bsstring.h"
#include "common/ll-common.h"
#include "context/context.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct AstNode* ast_allocate(enum AstType at) {
  struct AstNode* ast = malloc(sizeof(*ast));
  memset(ast, 0, sizeof(*ast));
  ast->at = at;
  return ast;
}

struct AstNode* ast_next(struct AstNode* ast) { return ast->next; }
void ast_append(struct AstNode** head, struct AstNode* tail) {
  ASSERT(head != NULL);
  LL_APPEND(*head, tail);
}
enum AstType ast_type(struct AstNode* ast) { return ast->at; }
int ast_is_type(struct AstNode* ast, enum AstType at) {
  return ast_type(ast) == at;
}
int ast_num_children(struct AstNode* ast) {
  enum AstType at = ast_type(ast);
  if(at == ast_FieldAccess) return 2;
  else if(at == ast_Type) return 2;
  else if(at == ast_Variable) return 3;
  else if(at == ast_Function) return 4;
  else if(at == ast_Assignment) return 2;
  else if(at == ast_Return) return 1;
  else if(at == ast_Block) return 1;
  else if(at == ast_Conditional) return 3;
  else if(at == ast_While) return 2;
  else if(at == ast_Expr) return 2;
  else if(at == ast_Call) return 2;
  return 0;
}

static void ast_type_to_string(char* buf, enum AstType at) {
  if(at == ast_Identifier) bsstrcpy(buf, "Identifier");
  else if(at == ast_Typename) bsstrcpy(buf, "Typename");
  else if(at == ast_FieldAccess) bsstrcpy(buf, "FieldAccess");
  else if(at == ast_Type) bsstrcpy(buf, "Type");
  else if(at == ast_Variable) bsstrcpy(buf, "Variable");
  else if(at == ast_Function) bsstrcpy(buf, "Function");
  else if(at == ast_Assignment) bsstrcpy(buf, "Assignment");
  else if(at == ast_Return) bsstrcpy(buf, "Return");
  else if(at == ast_Break) bsstrcpy(buf, "Break");
  else if(at == ast_Block) bsstrcpy(buf, "Block");
  else if(at == ast_Conditional) bsstrcpy(buf, "Conditional");
  else if(at == ast_While) bsstrcpy(buf, "While");
  else if(at == ast_Expr) bsstrcpy(buf, "Expr");
  else if(at == ast_Call) bsstrcpy(buf, "Call");
  else if(at == ast_Number) bsstrcpy(buf, "Number");
  else if(at == ast_String) bsstrcpy(buf, "String");
}

void OperatorType_tostring(char* buf, enum OperatorType op) {
  if(op == op_PLUS) bsstrcpy(buf, "PLUS");
  else if(op == op_MINUS) bsstrcpy(buf, "MINUS");
  else if(op == op_MULT) bsstrcpy(buf, "MULT");
  else if(op == op_DIVIDE) bsstrcpy(buf, "DIVIDE");
  else if(op == op_AND) bsstrcpy(buf, "AND");
  else if(op == op_OR) bsstrcpy(buf, "OR");
  else if(op == op_LT) bsstrcpy(buf, "LT");
  else if(op == op_GT) bsstrcpy(buf, "GT");
  else if(op == op_LTEQ) bsstrcpy(buf, "LTEQ");
  else if(op == op_GTEQ) bsstrcpy(buf, "GTEQ");
  else if(op == op_EQ) bsstrcpy(buf, "EQ");
  else if(op == op_NEQ) bsstrcpy(buf, "NEQ");
  else if(op == op_NOT) bsstrcpy(buf, "NOT");
  else if(op == op_CAST) bsstrcpy(buf, "CAST");
  else if(op == op_TAKE_ADDRESS) bsstrcpy(buf, "TAKE_ADDRESS");
  else if(op == op_PTR_DEREFERENCE) bsstrcpy(buf, "PTR_DEREFERENCE");
}
static void
ast_dump_helper(struct Context* context, struct AstNode* root, int indent) {

  if(root == NULL) return;
  ast_foreach(root, ast) {
#define PRINT_INDENT                                                           \
  do {                                                                         \
    for(int i = 0; i < indent; i++)                                            \
      printf(" ");                                                             \
  } while(0)

    char typestr[32];
    ast_type_to_string(typestr, ast_type(ast));

    PRINT_INDENT;
    printf("%s:\n", typestr);
    if(ast_is_type(ast, ast_Identifier)) {
      PRINT_INDENT;
      printf(" name='%s'\n", ast_Identifier_name(ast));
    } else if(ast_is_type(ast, ast_Typename)) {
      PRINT_INDENT;
      printf(" name='");
      printf("%s", ast_Typename_name(ast));
      for(int i = 0; i < ast_Typename_ptr_level(ast); i++)
        printf("*");
      printf("'\n");
    } else if(ast_is_type(ast, ast_Number)) {
      PRINT_INDENT;
      printf(" value=%d\n", ast_Number_value(ast));
    } else if(ast_is_type(ast, ast_String)) {
      PRINT_INDENT;
      printf(" value='%s'\n", ast_String_value(ast));
    } else if(ast_is_type(ast, ast_Expr) && ast_Expr_op(ast) != op_NONE) {
      char opstr[32];
      OperatorType_tostring(opstr, ast_Expr_op(ast));
      PRINT_INDENT;
      printf(" op='%s'\n", opstr);
    }
    ast_foreach_child(ast, c) { ast_dump_helper(context, c, indent + 2); }
  }
}
void dump_ast(struct Context* context) {
  ast_dump_helper(context, context->ast, 0);
}

void ast_verify_helper(struct Context* context, struct AstNode* ast) {
  if(ast == NULL) return;
  ast_foreach(ast, a) {
    switch(ast_type(a)) {
      case ast_Identifier:
        if(!ast_verify_Identifier(a)) {
          ERROR_ON_AST(context, a, "failed to verify Identifier\n");
        }
        break;
      case ast_Typename:
        if(!ast_verify_Typename(a)) {
          ERROR_ON_AST(context, a, "failed to verify Typename\n");
        }
        break;
      case ast_FieldAccess:
        if(!ast_verify_FieldAccess(a)) {
          ERROR_ON_AST(context, a, "failed to verify FieldAccess\n");
        }
        break;
      case ast_Type:
        if(!ast_verify_Type(a)) {
          ERROR_ON_AST(context, a, "failed to verify Type\n");
        }
        break;
      case ast_Variable:
        if(!ast_verify_Variable(a)) {
          ERROR_ON_AST(context, a, "failed to verify Variable\n");
        }
        break;
      case ast_Function:
        if(!ast_verify_Function(a)) {
          ERROR_ON_AST(context, a, "failed to verify Function\n");
        }
        break;
      case ast_Assignment:
        if(!ast_verify_Assignment(a)) {
          ERROR_ON_AST(context, a, "failed to verify Assignment\n");
        }
        break;
      case ast_Return:
        if(!ast_verify_Return(a)) {
          ERROR_ON_AST(context, a, "failed to verify Return\n");
        }
        break;
      case ast_Break:
        if(!ast_verify_Break(a)) {
          ERROR_ON_AST(context, a, "failed to verify Break\n");
        }
        break;
      case ast_Block:
        if(!ast_verify_Block(a)) {
          ERROR_ON_AST(context, a, "failed to verify Block\n");
        }
        break;
      case ast_Conditional:
        if(!ast_verify_Conditional(a)) {
          ERROR_ON_AST(context, a, "failed to verify Conditional\n");
        }
        break;
      case ast_While:
        if(!ast_verify_While(a)) {
          ERROR_ON_AST(context, a, "failed to verify While\n");
        }
        break;
      case ast_Expr:
        if(!ast_verify_Expr(a)) {
          ERROR_ON_AST(context, a, "failed to verify Expr\n");
        }
        break;
      case ast_Call:
        if(!ast_verify_Call(a)) {
          ERROR_ON_AST(context, a, "failed to verify Call\n");
        }
        break;
      case ast_Number:
        if(!ast_verify_Number(a)) {
          ERROR_ON_AST(context, a, "failed to verify Number\n");
        }
        break;
      case ast_String:
        if(!ast_verify_String(a)) {
          ERROR_ON_AST(context, a, "failed to verify String\n");
        }
        break;
    }
    ast_foreach_child(a, c) { ast_verify_helper(context, c); }
  }
}
void verify_ast(struct Context* context) {
  ast_verify_helper(context, context->ast);
}

struct AstNode* ast_build_Identifier(char* name) {
  struct AstNode* ast = ast_allocate(ast_Identifier);
  int len = strlen(name) + 1;
  ast->str_value = malloc(sizeof(*ast->str_value) * len);
  bsstrcpy(ast->str_value, name);
  return ast;
}
int ast_verify_Identifier(struct AstNode* ast) {
  return ast_is_type(ast, ast_Identifier);
}
char* ast_Identifier_name(struct AstNode* ast) { return ast->str_value; }

struct AstNode* ast_build_Typename(char* name) {
  return ast_build_Typename2(name, 0);
}
struct AstNode* ast_build_Typename2(char* name, int ptr_level) {
  struct AstNode* ast = ast_allocate(ast_Typename);
  int len = strlen(name) + 1;
  ast->str_value = malloc(sizeof(*ast->str_value) * len);
  bsstrcpy(ast->str_value, name);
  ast->int_value = ptr_level;
  return ast;
}
int ast_verify_Typename(struct AstNode* ast) {
  return ast_is_type(ast, ast_Typename);
}
char* ast_Typename_name(struct AstNode* ast) { return ast->str_value; }
int ast_Typename_ptr_level(struct AstNode* ast) { return ast->int_value; }

struct AstNode* ast_build_FieldAccess(
    struct AstNode* object,
    int object_is_ptr,
    struct AstNode* field) {
  struct AstNode* ast = ast_allocate(ast_FieldAccess);
  ast->children[0] = object;
  ast->children[1] = field;
  ast->int_value = object_is_ptr;
  return ast;
}
int ast_verify_FieldAccess(struct AstNode* ast) {
  return ast_is_type(ast, ast_FieldAccess) &&
         ast_is_type(ast_FieldAccess_object(ast), ast_Identifier) &&
         ast_is_type(ast_FieldAccess_field(ast), ast_Identifier);
}
struct AstNode* ast_FieldAccess_object(struct AstNode* ast) {
  return ast->children[0];
}
struct AstNode* ast_FieldAccess_field(struct AstNode* ast) {
  return ast->children[1];
}
int ast_FieldAccess_object_is_ptr(struct AstNode* ast) {
  return ast->int_value;
}

struct AstNode* ast_build_Type(struct AstNode* name, struct AstNode* args) {
  struct AstNode* ast = ast_allocate(ast_Type);
  ast->children[0] = name;
  ast->children[1] = args;
  ast->int_value = 0; // is_alias
  return ast;
}
struct AstNode*
ast_build_TypeAlias(struct AstNode* name, struct AstNode* alias) {
  struct AstNode* ast = ast_allocate(ast_Type);
  ast->children[0] = name;
  ast->children[1] = alias;
  ast->int_value = 1; // is_alias
  return ast;
}
int ast_verify_Type(struct AstNode* ast) {
  int is_type = ast_is_type(ast, ast_Type) &&
                ast_is_type(ast_Type_name(ast), ast_Typename);
  if(ast_Type_is_alias(ast)) {
    return is_type && ast_is_type(ast_Type_alias(ast), ast_Typename);
  } else {
    return is_type && (ast_Type_args(ast) == NULL ||
                       ast_is_type(ast_Type_args(ast), ast_Variable));
  }
}
struct AstNode* ast_Type_name(struct AstNode* ast) { return ast->children[0]; }
struct AstNode* ast_Type_args(struct AstNode* ast) { return ast->children[1]; }
struct AstNode* ast_Type_alias(struct AstNode* ast) { return ast->children[1]; }
int ast_Type_is_alias(struct AstNode* ast) { return ast->int_value ? 1 : 0; }

struct AstNode* ast_build_Variable(
    struct AstNode* name,
    struct AstNode* type,
    struct AstNode* expr) {
  struct AstNode* ast = ast_allocate(ast_Variable);
  ast->children[0] = name;
  ast->children[1] = type;
  ast->children[2] = expr;
  return ast;
}
int ast_verify_Variable(struct AstNode* ast) {
  return ast_is_type(ast, ast_Variable) &&
         ast_is_type(ast_Variable_name(ast), ast_Identifier) &&
         ast_is_type(ast_Variable_type(ast), ast_Typename) &&
         (ast_Variable_expr(ast) == NULL ||
          ast_is_type(ast_Variable_expr(ast), ast_Expr));
}
struct AstNode* ast_Variable_name(struct AstNode* ast) {
  return ast->children[0];
}
struct AstNode* ast_Variable_type(struct AstNode* ast) {
  return ast->children[1];
}
struct AstNode* ast_Variable_expr(struct AstNode* ast) {
  return ast->children[2];
}

struct AstNode* ast_build_FunctionHeader(
    struct AstNode* name,
    struct AstNode* args,
    struct AstNode* ret_type) {
  struct AstNode* ast = ast_allocate(ast_Function);
  ast->children[0] = name;
  ast->children[1] = args;
  ast->children[2] = ret_type;
  return ast;
}
struct AstNode* ast_build_ExternFunction(struct AstNode* header) {
  header->int_value = 1;
  return header;
}
struct AstNode* ast_build_ExportFunction(struct AstNode* header) {
  header->int_value = 2;
  return header;
}
struct AstNode*
ast_build_Function(struct AstNode* header, struct AstNode* body) {
  header->children[3] = body;
  return header;
}
int ast_verify_Function(struct AstNode* ast) {
  int valid_header = ast_is_type(ast, ast_Function) &&
                     ast_is_type(ast_Function_name(ast), ast_Identifier) &&
                     (ast_Function_args(ast) == NULL ||
                      ast_is_type(ast_Function_args(ast), ast_Variable)) &&
                     ast_is_type(ast_Function_ret_type(ast), ast_Typename);
  return valid_header;
}
struct AstNode* ast_Function_name(struct AstNode* ast) {
  return ast->children[0];
}
struct AstNode* ast_Function_args(struct AstNode* ast) {
  return ast->children[1];
}
int ast_Function_num_args(struct AstNode* ast) {
  struct AstNode* args = ast_Function_args(ast);
  int n = 0;
  ast_foreach(args, a) { n += 1; }
  return n;
}
struct AstNode* ast_Function_ret_type(struct AstNode* ast) {
  return ast->children[2];
}
struct AstNode* ast_Function_body(struct AstNode* ast) {
  return ast->children[3];
}
int ast_Function_has_body(struct AstNode* ast) {
  return ast_Function_body(ast) != NULL;
}
int ast_Function_is_extern(struct AstNode* ast) { return ast->int_value == 1; }
int ast_Function_is_export(struct AstNode* ast) { return ast->int_value == 2; }

struct AstNode* ast_build_Assignment(
    struct AstNode* lhs,
    struct AstNode* expr,
    int is_ptr_access) {
  struct AstNode* ast = ast_allocate(ast_Assignment);
  ast->children[0] = lhs;
  ast->children[1] = expr;
  ast->int_value = is_ptr_access;
  return ast;
}
int ast_verify_Assignment(struct AstNode* ast) {
  return ast_is_type(ast, ast_Assignment) &&
         (ast_is_type(ast_Assignment_lhs(ast), ast_Identifier) ||
          ast_is_type(ast_Assignment_lhs(ast), ast_FieldAccess)) &&
         ast_is_type(ast_Assignment_expr(ast), ast_Expr);
}
struct AstNode* ast_Assignment_lhs(struct AstNode* ast) {
  return ast->children[0];
}
int ast_Assignment_is_ptr_access(struct AstNode* ast) { return ast->int_value; }
struct AstNode* ast_Assignment_expr(struct AstNode* ast) {
  return ast->children[1];
}

struct AstNode* ast_build_Return(struct AstNode* expr) {
  struct AstNode* ast = ast_allocate(ast_Return);
  ast->children[0] = expr;
  return ast;
}
int ast_verify_Return(struct AstNode* ast) {
  return ast_is_type(ast, ast_Return) &&
         (ast_Return_expr(ast) == NULL ||
          ast_is_type(ast_Return_expr(ast), ast_Expr));
}
struct AstNode* ast_Return_expr(struct AstNode* ast) {
  return ast->children[0];
}

struct AstNode* ast_build_Break() {
  struct AstNode* ast = ast_allocate(ast_Break);
  return ast;
}
int ast_verify_Break(struct AstNode* ast) {
  return ast_is_type(ast, ast_Break);
}

struct AstNode* ast_build_Block(struct AstNode* stmts) {
  struct AstNode* ast = ast_allocate(ast_Block);
  ast->children[0] = stmts;
  return ast;
}
struct AstNode* ast_build_EmptyBlock() { return ast_build_Block(NULL); }
int ast_verify_Block(struct AstNode* ast) {
  return ast_is_type(ast, ast_Block);
}
struct AstNode* ast_Block_stmts(struct AstNode* ast) {
  return ast->children[0];
}
int ast_Block_is_empty(struct AstNode* ast) {
  return ast_Block_stmts(ast) == NULL;
}

struct AstNode* ast_build_Conditional(
    struct AstNode* condition,
    struct AstNode* if_body,
    struct AstNode* else_body) {
  struct AstNode* ast = ast_allocate(ast_Conditional);
  ast->children[0] = condition;
  ast->children[1] = if_body;
  ast->children[2] = else_body;
  return ast;
}
int ast_verify_Conditional(struct AstNode* ast) {
  return ast_is_type(ast, ast_Conditional) &&
         ast_is_type(ast_Conditional_condition(ast), ast_Expr) &&
         ast_Conditional_if_body(ast) != NULL &&
         ast_Conditional_else_body(ast) != NULL;
  ;
  ;
}
struct AstNode* ast_Conditional_condition(struct AstNode* ast) {
  return ast->children[0];
}
struct AstNode* ast_Conditional_if_body(struct AstNode* ast) {
  return ast->children[1];
}
struct AstNode* ast_Conditional_else_body(struct AstNode* ast) {
  return ast->children[2];
}

struct AstNode*
ast_build_While(struct AstNode* condition, struct AstNode* body) {
  struct AstNode* ast = ast_allocate(ast_While);
  ast->children[0] = condition;
  ast->children[1] = body;
  return ast;
}
int ast_verify_While(struct AstNode* ast) {
  return ast_is_type(ast, ast_While) &&
         ast_is_type(ast_While_condition(ast), ast_Expr) &&
         ast_While_body(ast) != NULL;
}

struct AstNode* ast_While_condition(struct AstNode* ast) {
  return ast->children[0];
}
struct AstNode* ast_While_body(struct AstNode* ast) { return ast->children[1]; }

struct AstNode* ast_build_Expr_plain(struct AstNode* lhs) {
  return ast_build_Expr_uop(lhs, op_NONE);
}
struct AstNode* ast_build_Expr_binop(
    struct AstNode* lhs,
    struct AstNode* rhs,
    enum OperatorType op) {
  struct AstNode* ast = ast_allocate(ast_Expr);
  ast->children[0] = lhs;
  ast->children[1] = rhs;
  ast->int_value = (int)op;
  return ast;
}
struct AstNode* ast_build_Expr_uop(struct AstNode* lhs, enum OperatorType op) {
  return ast_build_Expr_binop(lhs, NULL, op);
}
int ast_verify_Expr(struct AstNode* ast) {
  int valid_atom = ast_Expr_lhs(ast) != NULL;
  int valid_infix = ast_Expr_lhs(ast) != NULL && ast_Expr_rhs(ast) != NULL;
  int valid_prefix = ast_Expr_lhs(ast) != NULL;
  return ast_is_type(ast, ast_Expr) &&
         (valid_atom || valid_infix || valid_prefix);
}
struct AstNode* ast_Expr_lhs(struct AstNode* ast) { return ast->children[0]; }
struct AstNode* ast_Expr_rhs(struct AstNode* ast) { return ast->children[1]; }
enum OperatorType ast_Expr_op(struct AstNode* ast) {
  enum OperatorType op = (enum OperatorType)ast->int_value;
  return op;
}
int ast_Expr_is_plain(struct AstNode* ast) {
  return ast_Expr_op(ast) == op_NONE;
}
int ast_Expr_is_binop(struct AstNode* ast) { return ast_Expr_rhs(ast) != NULL; }
int ast_Expr_is_uop(struct AstNode* ast) { return ast_Expr_rhs(ast) == NULL; }

struct AstNode* ast_build_Call(struct AstNode* name, struct AstNode* args) {
  struct AstNode* ast = ast_allocate(ast_Call);
  ast->children[0] = name;
  ast->children[1] = args;
  return ast;
}
int ast_verify_Call(struct AstNode* ast) {
  return ast_is_type(ast, ast_Call) &&
         ast_is_type(ast_Call_name(ast), ast_Identifier) &&
         (ast_Call_args(ast) == NULL ||
          ast_is_type(ast_Call_args(ast), ast_Expr));
}
struct AstNode* ast_Call_name(struct AstNode* ast) { return ast->children[0]; }
struct AstNode* ast_Call_args(struct AstNode* ast) { return ast->children[1]; }

int ast_Call_num_args(struct AstNode* ast) {
  struct AstNode* args = ast_Call_args(ast);
  int n = 0;
  ast_foreach(args, a) { n += 1; }
  return n;
}

struct AstNode* ast_build_Number(int value) {
  struct AstNode* ast = ast_allocate(ast_Number);
  ast->int_value = value;
  return ast;
}
int ast_verify_Number(struct AstNode* ast) {
  return ast_is_type(ast, ast_Number);
}
int ast_Number_value(struct AstNode* ast) { return ast->int_value; }

struct AstNode* ast_build_String(char* value) {
  struct AstNode* ast = ast_allocate(ast_String);
  int len = strlen(value) + 1;
  ast->str_value = malloc(sizeof(*ast->str_value) * len);
  bsstrcpy(ast->str_value, value);
  return ast;
}
int ast_verify_String(struct AstNode* ast) {
  return ast_is_type(ast, ast_String);
}
char* ast_String_value(struct AstNode* ast) { return ast->str_value; }
