lexer grammar PeblLexer;

EXTERN: 'extern';
EXPORT: 'export';
FUNC: 'func';
TYPE: 'type';
LET: 'let';
IF: 'if';
ELSE: 'else';
WHILE: 'while';
RETURN: 'return';
BREAK: 'break';

LPAREN: '(';
RPAREN: ')';
LCURLY: '{';
RCURLY: '}';

COLON: ':';
SEMICOLON: ';';
COMMA: ',';

DOT: '.';
EQUALS: '=';
ARROW: '->';
STAR: '*';
PLUS: '+';
MINUS: '-';
DIVIDE: '/';
AND: '&&';
OR: '||';
LT: '<';
GT: '>';
LTEQ: '<=';
GTEQ: '>=';
EQ: '==';
NEQ: '!=';
AMPERSAND: '&';
NOT: '!';

ID: [a-zA-Z_][a-zA-Z_0-9]*;
CHAR_LITERAL: '\'' '\\'? . '\'';
STRING_LITERAL: '"' ~["]* '"';
NUMBER: [0-9]+;
TRUE: 'true';
FALSE: 'false';

WS: [ \n\t\r]+ -> skip;
COMMENT: '#' ~[\n]* -> skip;
