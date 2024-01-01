#include "parser/parser.h"

#include "ast/location.h"
#include "context/context.h"

#include <stdlib.h>

static struct AstNode* parse_statement_list(struct Context* context);
static struct AstNode* parse_statement(struct Context* context);
static struct AstNode* parse_block_statement(struct Context* context);
static struct AstNode* parse_function_def(struct Context* context);
static struct AstNode* parse_function_header(struct Context* context);
static struct AstNode* parse_body(struct Context* context);
static struct AstNode* parse_args(struct Context* context);
static struct AstNode* parse_name_with_type(struct Context* context);
static struct AstNode* parse_type_def(struct Context* context);
static struct AstNode* parse_type_list(struct Context* context);
static struct AstNode* parse_var_def(struct Context* context);
static struct AstNode* parse_varname(struct Context* context);
static struct AstNode* parse_typename(struct Context* context);
static struct AstNode* parse_expr(struct Context* context);
static struct AstNode* parse_expr_list(struct Context* context);
static struct AstNode* parse_literal(struct Context* context);
static struct AstNode* parse_atom(struct Context* context);
static enum OperatorType parse_op(struct Context* context);
static enum OperatorType parse_preop(struct Context* context);
static struct AstNode* parse_call_stmt(struct Context* context);
static struct AstNode* parse_call_expr(struct Context* context);
static struct AstNode* parse_assignment(struct Context* context);
static struct AstNode* parse_if_stmt(struct Context* context);
static struct AstNode* parse_while_stmt(struct Context* context);
static struct AstNode* parse_return_stmt(struct Context* context);
static struct AstNode* parse_break_stmt(struct Context* context);

__attribute__((noreturn)) static void
syntax_error(struct Context* context, struct lexer_token* t) {
  ERROR_ON_LINE(
      context,
      LT_lineno(t),
      "syntax error on token {%s, '%s'}\n",
      tokentype_to_string(LT_type(t)),
      LT_lexeme(t));
}
static struct lexer_token*
expect(struct Context* context, enum lexer_tokentype tt) {
  struct lexer_token* t = lexer_gettoken(context);
  if(LT_type(t) != tt) syntax_error(context, t);
  return t;
}

static void add_location_for_token(
    struct Context* context,
    struct AstNode* ast,
    struct lexer_token* t) {
  Context_build_location(context, ast, LT_lineno(t), LT_lineno(t));
}

// statement_list -> EPSILON | statement | statement statement_list
static struct AstNode* parse_statement_list(struct Context* context) {
  struct lexer_token* t = lexer_peek(context, 1);
  if(LT_type(t) == tt_FUNC || LT_type(t) == tt_EXTERN || LT_type(t) == tt_EXPORT ||
     LT_type(t) == tt_TYPE || LT_type(t) == tt_LET || LT_type(t) == tt_ID ||
     LT_type(t) == tt_STAR || LT_type(t) == tt_IF || LT_type(t) == tt_WHILE ||
     LT_type(t) == tt_RETURN || LT_type(t) == tt_BREAK) {
    struct AstNode* head = parse_statement(context);
    struct AstNode* tail = parse_statement_list(context);
    ast_append(&head, tail);
    return head;
  } else if(LT_type(t) == tt_ERROR) {
    syntax_error(context, t);
  }
  return NULL;
}
// statement -> function_def | type_def | var_def |
// block_statement
static struct AstNode* parse_statement(struct Context* context) {
  struct lexer_token* t = lexer_peek(context, 1);
  if(LT_type(t) == tt_FUNC || LT_type(t) == tt_EXPORT || LT_type(t) == tt_EXTERN) {
    return parse_function_def(context);
  } else if(LT_type(t) == tt_TYPE) {
    return parse_type_def(context);
  } else if(LT_type(t) == tt_LET) {
    return parse_var_def(context);
  } else {
    return parse_block_statement(context);
  }
}
// block_statement -> assignment | if_stmt | while_stmt | return_stmt |
// break_stmt | call_stmt
static struct AstNode* parse_block_statement(struct Context* context) {
  struct lexer_token* t = lexer_peek(context, 1);
  if(LT_type(t) == tt_IF) {
    return parse_if_stmt(context);
  } else if(LT_type(t) == tt_WHILE) {
    return parse_while_stmt(context);
  } else if(LT_type(t) == tt_RETURN) {
    return parse_return_stmt(context);
  } else if(LT_type(t) == tt_BREAK) {
    return parse_break_stmt(context);
  } else {
    if(lexer_peek(context, 2)->tt == tt_LPAREN) {
      return parse_call_stmt(context);
    } else {
      return parse_assignment(context);
    }
  }
}
// function_def -> (EXTERN|EXPORT)? function_header (body|SEMICOLON)
static struct AstNode* parse_function_def(struct Context* context) {
  // cxan only be extern or export
  int is_extern = 0;
  int is_export = 0;
  if(lexer_peek(context, 1)->tt == tt_EXTERN) {
    is_extern = 1;
    lexer_gettoken(context);
  } else if(lexer_peek(context, 1)->tt == tt_EXPORT) {
    is_export = 1;
    lexer_gettoken(context);
  }
  struct AstNode* header = parse_function_header(context);
  struct AstNode* body = NULL;
  if(lexer_peek(context, 1)->tt == tt_SEMICOLON) {
    // no body
    // if export, ERROR
    if(is_export) {
      ERROR_ON_AST(context, header, "cannot export a function with no body");
    }
    expect(context, tt_SEMICOLON);
  } else {
    // if extern, ERROR
    if(is_extern) {
      ERROR_ON_AST(
          context,
          header,
          "cannot declare a extern function with a body");
    }
    body = parse_body(context);
  }
  // body can be null
  struct AstNode* func = ast_build_Function(header, body);
  if(is_export) {
    func = ast_build_ExportFunction(func);
  } else if(is_extern) {
    func = ast_build_ExternFunction(func);
  }

  return func;
}
// function_header -> FUNC varname LPAREN args RPAREN COLON typename
static struct AstNode* parse_function_header(struct Context* context) {
  struct lexer_token* t = expect(context, tt_FUNC);
  struct AstNode* name = parse_varname(context);
  expect(context, tt_LPAREN);
  struct AstNode* args = parse_args(context);
  expect(context, tt_RPAREN);
  expect(context, tt_COLON);
  struct AstNode* ret_type = parse_typename(context);
  struct AstNode* func_node = ast_build_FunctionHeader(name, args, ret_type);
  add_location_for_token(context, func_node, t);
  return func_node;
}
// body -> LCURLY statement_list RCURLY
static struct AstNode* parse_body(struct Context* context) {
  expect(context, tt_LCURLY);
  struct AstNode* body = parse_statement_list(context);
  expect(context, tt_RCURLY);
  return ast_build_Block(body);
}
// args -> EPSILON | name_with_type | name_with_type COMMA args
static struct AstNode* parse_args(struct Context* context) {
  struct lexer_token* t = lexer_peek(context, 1);
  if(LT_type(t) == tt_ID) {
    struct AstNode* head = parse_name_with_type(context);
    t = lexer_peek(context, 1);
    if(LT_type(t) == tt_COMMA) {
      expect(context, tt_COMMA);
      struct AstNode* tail = parse_args(context);
      ast_append(&head, tail);
    }
    return head;
  }
  return NULL;
}
// name_with_type -> varname COLON typename
static struct AstNode* parse_name_with_type(struct Context* context) {
  struct AstNode* name = parse_varname(context);
  struct lexer_token* t = expect(context, tt_COLON);
  struct AstNode* type = parse_typename(context);
  struct AstNode* var_node = ast_build_Variable(name, type, NULL);
  add_location_for_token(context, var_node, t);
  return var_node;
}
// type_def -> TYPE varname EQUALS LCURLY type_list RCURLY | TYPE varname EQUALS
// typename SEMICOLON | TYPE varname SEMICOLON
static struct AstNode* parse_type_def(struct Context* context) {
  struct lexer_token* type_tok = expect(context, tt_TYPE);
  struct AstNode* name = parse_varname(context);
  if(lexer_peek(context, 1)->tt == tt_EQUALS) {
    expect(context, tt_EQUALS);
    struct lexer_token* t = lexer_peek(context, 1);
    if(LT_type(t) == tt_LCURLY) {
      expect(context, tt_LCURLY);
      struct AstNode* args = parse_type_list(context);
      expect(context, tt_RCURLY);
      struct AstNode* type_node = ast_build_Type(name, args);
      add_location_for_token(context, type_node, type_tok);
      return type_node;
    } else {
      struct AstNode* alias = parse_typename(context);
      expect(context, tt_SEMICOLON);
      struct AstNode* type_node = ast_build_TypeAlias(name, alias);
      add_location_for_token(context, type_node, type_tok);
      return type_node;
    }
  } else {
    expect(context, tt_SEMICOLON);
    struct AstNode* type_node = ast_build_OpaqueType(name);
    add_location_for_token(context, type_node, type_tok);
    return type_node;
  }
}
// type_list -> EPSILON | name_with_type SEMICOLON type_list
static struct AstNode* parse_type_list(struct Context* context) {
  struct lexer_token* t = lexer_peek(context, 1);
  if(LT_type(t) == tt_ID) {
    struct AstNode* head = parse_name_with_type(context);
    expect(context, tt_SEMICOLON);
    struct AstNode* tail = parse_type_list(context);
    ast_append(&head, tail);
    return head;
  }
  return NULL;
}
// var_def -> LET varname (COLON typename)? (EQUALS expr)? SEMICOLON
static struct AstNode* parse_var_def(struct Context* context) {
  struct lexer_token* t = expect(context, tt_LET);
  struct AstNode* var = parse_varname(context);
  struct AstNode* type = NULL;
  struct AstNode* expr = NULL;
  if(lexer_peek(context, 1)->tt == tt_COLON) {
    expect(context, tt_COLON);
    type = parse_typename(context);
  }
  if(lexer_peek(context, 1)->tt == tt_EQUALS) {
    expect(context, tt_EQUALS);
    expr = parse_expr(context);
  }
  expect(context, tt_SEMICOLON);
  struct AstNode* var_node = ast_build_Variable(var, type, expr);
  add_location_for_token(context, var_node, t);
  return var_node;
}
// varname -> ID
static struct AstNode* parse_varname(struct Context* context) {
  struct lexer_token* t = expect(context, tt_ID);
  struct AstNode* ident = ast_build_Identifier(LT_lexeme(t));
  add_location_for_token(context, ident, t);
  return ident;
}
// typename -> ID STAR* | TYPE
static struct AstNode* parse_typename(struct Context* context) {
  if(lexer_peek(context, 1)->tt == tt_TYPE) {
    struct lexer_token* t = expect(context, tt_TYPE);
    struct AstNode* typename = ast_build_Typename("type");
    add_location_for_token(context, typename, t);
    return typename;
  } else {
    struct lexer_token* t = expect(context, tt_ID);
    int ptr_level = 0;
    while(lexer_peek(context, 1)->tt == tt_STAR) {
      expect(context, tt_STAR);
      ptr_level++;
    }
    struct AstNode* typename = ast_build_Typename2(LT_lexeme(t), ptr_level);
    add_location_for_token(context, typename, t);
    return typename;
  }
}
// expr -> atom | atom op atom | preop atom
static struct AstNode* parse_expr(struct Context* context) {
  struct lexer_token* t = lexer_peek(context, 1);
  if(LT_type(t) == tt_AMPERSAND || LT_type(t) == tt_STAR || LT_type(t) == tt_NOT) {
    enum OperatorType op = parse_preop(context);
    struct AstNode* lhs = parse_atom(context);
    struct AstNode* expr_node = ast_build_Expr_uop(lhs, op);
    add_location_for_token(context, expr_node, t);
    return expr_node;
  } else {
    struct lexer_token* tok_for_loc = t;
    struct AstNode* lhs = parse_atom(context);
    t = lexer_peek(context, 1);
    if(LT_type(t) == tt_PLUS || LT_type(t) == tt_MINUS || LT_type(t) == tt_STAR ||
       LT_type(t) == tt_DIVIDE || LT_type(t) == tt_AND || LT_type(t) == tt_OR ||
       LT_type(t) == tt_LT || LT_type(t) == tt_GT || LT_type(t) == tt_LTEQ ||
       LT_type(t) == tt_GTEQ || LT_type(t) == tt_EQ || LT_type(t) == tt_NEQ ||
       LT_type(t) == tt_COLON) {
      enum OperatorType op = parse_op(context);
      struct AstNode* rhs = parse_atom(context);
      struct AstNode* expr_node = ast_build_Expr_binop(lhs, rhs, op);
      add_location_for_token(context, expr_node, tok_for_loc);
      return expr_node;
    } else {
      struct AstNode* expr_node = ast_build_Expr_plain(lhs);
      add_location_for_token(context, expr_node, tok_for_loc);
      return expr_node;
    }
  }
}

static int is_literal(struct lexer_token* t) {
  return LT_type(t) == tt_NUMBER || LT_type(t) == tt_STRING_LITERAL ||
         LT_type(t) == tt_CHAR_LITERAL || LT_type(t) == tt_TRUE || LT_type(t) == tt_FALSE;
}

// expr_list -> EPSILON | expr | expr COMMA expr_list
static struct AstNode* parse_expr_list(struct Context* context) {
  struct lexer_token* t = lexer_peek(context, 1);
  if(LT_type(t) == tt_AMPERSAND || LT_type(t) == tt_STAR || LT_type(t) == tt_NOT ||
     is_literal(t) || LT_type(t) == tt_ID || LT_type(t) == tt_LPAREN) {
    struct AstNode* head = parse_expr(context);
    t = lexer_peek(context, 1);
    if(LT_type(t) == tt_COMMA) {
      expect(context, tt_COMMA);
      struct AstNode* tail = parse_expr_list(context);
      ast_append(&head, tail);
    }
    return head;
  }
  return NULL;
}
// literal -> NUMBER | STRING_LITERAL | CHAR_LITERAL | TRUE | FALSE
static struct AstNode* parse_literal(struct Context* context) {
  struct lexer_token* t = lexer_peek(context, 1);
  if(LT_type(t) == tt_NUMBER) {
    t = expect(context, tt_NUMBER);
    struct AstNode* num_node = ast_build_Number(atoi(LT_lexeme(t)), 64);
    add_location_for_token(context, num_node, t);
    return num_node;
  } else if(LT_type(t) == tt_STRING_LITERAL) {
    t = expect(context, tt_STRING_LITERAL);
    struct AstNode* string_node = ast_build_String(LT_lexeme(t));
    add_location_for_token(context, string_node, t);
    return string_node;
  } else if(LT_type(t) == tt_CHAR_LITERAL) {
    t = expect(context, tt_CHAR_LITERAL);
    struct AstNode* char_node = ast_build_Number((int)LT_lexeme(t)[0], 8);
    add_location_for_token(context, char_node, t);
    return char_node;
  } else if(LT_type(t) == tt_TRUE) {
    t = expect(context, tt_TRUE);
    struct AstNode* bool_node = ast_build_Number(1, 1);
    add_location_for_token(context, bool_node, t);
    return bool_node;
  } else if(LT_type(t) == tt_FALSE) {
    t = expect(context, tt_FALSE);
    struct AstNode* bool_node = ast_build_Number(0, 1);
    add_location_for_token(context, bool_node, t);
    return bool_node;
  } else {
    syntax_error(context, t);
  }
}
// atom -> literal | varname | call_expr | varname (DOT|ARROW) varname | LPAREN
// expr RPAREN
static struct AstNode* parse_atom(struct Context* context) {
  struct lexer_token* t = lexer_peek(context, 1);
  if(is_literal(t)) {
    return parse_literal(context);
  } else if(LT_type(t) == tt_ID) {
    if(lexer_peek(context, 2)->tt == tt_LPAREN) {
      return parse_call_expr(context);
    } else {
      struct AstNode* var = parse_varname(context);
      t = lexer_peek(context, 1);
      if(LT_type(t) == tt_DOT || LT_type(t) == tt_ARROW) {
        lexer_gettoken(context); // consume token
        struct lexer_token* dot_tok = t;
        struct AstNode* field = parse_varname(context);
        int object_is_ptr = LT_type(dot_tok) == tt_ARROW;
        struct AstNode* field_node =
            ast_build_FieldAccess(var, object_is_ptr, field);
        add_location_for_token(context, field_node, dot_tok);
        return field_node;
      } else {
        return var;
      }
    }
  } else if(LT_type(t) == tt_LPAREN) {
    struct lexer_token* tok = expect(context, tt_LPAREN);
    struct AstNode* expr = parse_expr(context);
    expect(context, tt_RPAREN);
    struct AstNode* wrapped_expr = ast_build_Expr_plain(expr);
    add_location_for_token(context, wrapped_expr, tok);
    return wrapped_expr;
  } else {
    syntax_error(context, t);
  }
}
// op -> PLUS | MINUS | STAR | DIVIDE | AND | OR | LT | GT | LTEQ | GTEQ | EQ |
// NEQ | COLON
static enum OperatorType parse_op(struct Context* context) {
  struct lexer_token* t = lexer_gettoken(context);
  if(LT_type(t) == tt_PLUS) {
    return op_PLUS;
  } else if(LT_type(t) == tt_MINUS) {
    return op_MINUS;
  } else if(LT_type(t) == tt_STAR) {
    return op_MULT;
  } else if(LT_type(t) == tt_DIVIDE) {
    return op_DIVIDE;
  } else if(LT_type(t) == tt_AND) {
    return op_AND;
  } else if(LT_type(t) == tt_OR) {
    return op_OR;
  } else if(LT_type(t) == tt_LT) {
    return op_LT;
  } else if(LT_type(t) == tt_GT) {
    return op_GT;
  } else if(LT_type(t) == tt_LTEQ) {
    return op_LTEQ;
  } else if(LT_type(t) == tt_GTEQ) {
    return op_GTEQ;
  } else if(LT_type(t) == tt_EQ) {
    return op_EQ;
  } else if(LT_type(t) == tt_NEQ) {
    return op_NEQ;
  } else if(LT_type(t) == tt_COLON) {
    return op_CAST;
  } else {
    syntax_error(context, t);
  }
}
// preop -> AMPERSAND | STAR | NOT
static enum OperatorType parse_preop(struct Context* context) {
  struct lexer_token* t = lexer_gettoken(context);
  if(LT_type(t) == tt_AMPERSAND) {
    return op_TAKE_ADDRESS;
  } else if(LT_type(t) == tt_STAR) {
    return op_PTR_DEREFERENCE;
  } else if(LT_type(t) == tt_NOT) {
    return op_NOT;
  } else {
    syntax_error(context, t);
  }
}
// call_stmt -> call_expr SEMICOLON
static struct AstNode* parse_call_stmt(struct Context* context) {
  struct AstNode* call_node = parse_call_expr(context);
  expect(context, tt_SEMICOLON);
  return call_node;
}
// call_expr -> varname LPAREN expr_list RPAREN
static struct AstNode* parse_call_expr(struct Context* context) {
  struct lexer_token* tok_for_loc = lexer_peek(context, 1);
  struct AstNode* var = parse_varname(context);
  expect(context, tt_LPAREN);
  struct AstNode* args = parse_expr_list(context);
  expect(context, tt_RPAREN);
  struct AstNode* call_node = ast_build_Call(var, args);
  add_location_for_token(context, call_node, tok_for_loc);
  return call_node;
}

// assignment -> STAR? varname ((DOT|ARROW) varname)? EQUALS expr SEMICOLON
static struct AstNode* parse_assignment(struct Context* context) {
  struct lexer_token* lhs_tok = lexer_peek(context, 1);
  int is_ptr_deref = 0;
  if(LT_type(lhs_tok) == tt_STAR) {
    expect(context, tt_STAR);
    is_ptr_deref = 1;
  }
  struct AstNode* lhs = parse_varname(context);
  struct lexer_token* dot_tok = lexer_peek(context, 1);
  if(LT_type(dot_tok) == tt_DOT || LT_type(dot_tok) == tt_ARROW) {
    lexer_gettoken(context); // consume token
    struct AstNode* field = parse_varname(context);
    int object_is_ptr = LT_type(dot_tok) == tt_ARROW;
    lhs = ast_build_FieldAccess(lhs, object_is_ptr, field);
    add_location_for_token(context, lhs, dot_tok);
  }
  expect(context, tt_EQUALS);
  struct AstNode* expr = parse_expr(context);
  expect(context, tt_SEMICOLON);

  struct AstNode* assign_node = ast_build_Assignment(lhs, expr, is_ptr_deref);
  add_location_for_token(context, assign_node, lhs_tok);
  return assign_node;
}
// if_stmt -> IF expr body (ELSE (body | if_stmt))?
static struct AstNode* parse_if_stmt(struct Context* context) {
  struct lexer_token* if_tok = expect(context, tt_IF);
  struct AstNode* cond = parse_expr(context);
  struct AstNode* if_body = parse_body(context);
  struct lexer_token* t = lexer_peek(context, 1);
  if(LT_type(t) == tt_ELSE) {
    expect(context, tt_ELSE);
    struct AstNode* else_body = NULL;
    if(lexer_peek(context, 1)->tt == tt_IF) {
      struct AstNode* nested_if = parse_if_stmt(context);
      else_body = ast_build_Block(nested_if);
    } else {
      else_body = parse_body(context);
    }
    struct AstNode* if_stmt = ast_build_Conditional(cond, if_body, else_body);
    add_location_for_token(context, if_stmt, if_tok);
    return if_stmt;
  } else {
    struct AstNode* if_stmt =
        ast_build_Conditional(cond, if_body, ast_build_EmptyBlock());
    add_location_for_token(context, if_stmt, if_tok);
    return if_stmt;
  }
}
// while_stmt -> WHILE expr body
static struct AstNode* parse_while_stmt(struct Context* context) {
  struct lexer_token* while_tok = expect(context, tt_WHILE);
  struct AstNode* cond = parse_expr(context);
  struct AstNode* body = parse_body(context);
  struct AstNode* while_node = ast_build_While(cond, body);
  add_location_for_token(context, while_node, while_tok);
  return while_node;
}
// return_stmt -> RETURN expr? SEMICOLON
static struct AstNode* parse_return_stmt(struct Context* context) {
  struct lexer_token* ret_tok = expect(context, tt_RETURN);
  struct AstNode* expr = NULL;
  if(lexer_peek(context, 1)->tt != tt_SEMICOLON) {
    expr = parse_expr(context);
  }
  expect(context, tt_SEMICOLON);
  struct AstNode* ret_node = ast_build_Return(expr);
  add_location_for_token(context, ret_node, ret_tok);
  return ret_node;
}
// break_stmt -> BREAK SEMICOLON
static struct AstNode* parse_break_stmt(struct Context* context) {
  struct lexer_token* t_break = expect(context, tt_BREAK);
  expect(context, tt_SEMICOLON);
  struct AstNode* break_node = ast_build_Break();
  add_location_for_token(context, break_node, t_break);
  return break_node;
}

void parser_init(__attribute__((unused)) struct Context* context) {}
void parser_parse(struct Context* context) {
  struct AstNode* body = parse_statement_list(context);
  struct AstNode* block = ast_build_Block(body);
  context->ast = block;
}
