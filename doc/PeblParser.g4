parser grammar PeblParser;
options {
  tokenVocab = PeblLexer;
}

program: statement_list;
statement_list: statement | statement statement_list |;
statement: function_def | type_def | var_def | block_statement;
block_statement:
  assignment
  | if_stmt
  | while_stmt
  | return_stmt
  | break_stmt
  | call_stmt
;

function_def: (EXTERN | EXPORT)? function_header body?;
function_header: FUNC varname LPAREN args RPAREN COLON typename;

body: LCURLY statement_list RCURLY;

args: name_with_type | name_with_type COMMA args |;
name_with_type: varname COLON typename;

type_def:
  TYPE typename EQUALS LCURLY type_list RCURLY
  | TYPE typename EQUALS typename SEMICOLON
  | TYPE typename SEMICOLON
;
type_list: name_with_type SEMICOLON type_list |;

var_def: LET varname COLON typename (EQUALS expr)? SEMICOLON;
varname: ID;
typename: ID STAR*;

expr: atom | atom op atom | preop atom;
expr_list: expr | expr COMMA expr_list |;
atom:
  NUMBER
  | STRING_LITERAL
  | varname
  | call_expr
  | varname (DOT | ARROW) varname
  | LPAREN expr RPAREN
;
op:
  PLUS
  | MINUS
  | STAR
  | DIVIDE
  | AND
  | OR
  | LT
  | GT
  | LTEQ
  | GTEQ
  | EQ
  | NEQ
  | COLON
;
preop: AMPERSAND | STAR | NOT;

call_stmt: call_expr SEMICOLON;
call_expr: varname LPAREN expr_list RPAREN;

assignment:
  STAR? varname ((DOT | ARROW) varname)? EQUALS expr SEMICOLON
;
if_stmt: IF expr body (ELSE body)?;
while_stmt: WHILE expr body;
return_stmt: RETURN expr? SEMICOLON;
break_stmt: BREAK SEMICOLON;
