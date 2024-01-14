# Grammar

The grammar is specified in semi-EBNF format for each of reading

```
statement_list -> EPSILON | statement | statement statement_list
statement -> function_def | type_def | var_def | block_statement
block_statement -> assignment | if_stmt | while_stmt | return_stmt | break_stmt | call_stmt

function_def -> (EXTERN|EXPORT)? function_header (body|SEMICOLON)
function_header -> FUNC varname LPAREN args RPAREN COLON typename

body -> LCURLY statement_list RCURLY

args -> EPSILON | name_with_type | name_with_type COMMA args
name_with_type -> varname COLON typename

type_def -> TYPE varname EQUALS LCURLY type_list RCURLY | TYPE varname EQUALS typename SEMICOLON | TYPE varname SEMICOLON
type_list -> EPSILON | name_with_type SEMICOLON type_list

var_def -> LET varname (COLON typename)? (EQUALS expr)? SEMICOLON
varname -> ID
typename -> ID STAR* | TYPE

expr -> atom | atom op atom | preop atom
expr_list -> EPSILON | expr | expr COMMA expr_list
literal -> NUMBER | STRING_LITERAL | CHAR_LITERAL | TRUE | FALSE
atom -> literal | varname | call_expr | varname (DOT|ARROW) varname | LPAREN expr RPAREN
op -> PLUS | MINUS | STAR | DIVIDE | AND | OR | LT | GT | LTEQ | GTEQ | EQ | NEQ | COLON
preop -> AMPERSAND | STAR | NOT | MINUS

call_stmt -> call_expr SEMICOLON
call_expr -> varname LPAREN expr_list RPAREN

assignment -> STAR? varname ((DOT | ARROW) varname)? EQUALS expr SEMICOLON
if_stmt -> IF expr body (ELSE (body | if_stmt))?
while_stmt -> WHILE expr body
return_stmt -> RETURN expr? SEMICOLON
break_stmt -> BREAK SEMICOLON
```
