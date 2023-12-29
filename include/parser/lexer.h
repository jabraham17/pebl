#ifndef PEBL_LEXER_H_
#define PEBL_LEXER_H_

#include "context/context.h"

#include <stdio.h>

struct lexer_token;

struct lexer_state {
  FILE* fp;
  struct lexer_token* peeked[2];
  int current_line;
};

#define LEXER_LEXEME_SIZE 64

enum lexer_tokentype {
  tt_EOF,
  tt_ERROR,
  tt_ID,
  tt_NUMBER,
  tt_TRUE,
  tt_FALSE,
  tt_STRING_LITERAL,
  tt_CHAR_LITERAL,
  tt_LPAREN,
  tt_RPAREN,
  tt_LCURLY,
  tt_RCURLY,
  tt_COMMA,
  tt_DOT,
  tt_ARROW,
  tt_COLON,
  tt_SEMICOLON,
  tt_FUNC,
  tt_EXTERN,
  tt_EXPORT,
  tt_TYPE,
  tt_LET,
  tt_PLUS,
  tt_MINUS,
  tt_STAR,
  tt_DIVIDE,
  tt_AND,
  tt_OR,
  tt_LT,
  tt_GT,
  tt_LTEQ,
  tt_GTEQ,
  tt_EQ,
  tt_NEQ,
  tt_EQUALS,
  tt_AMPERSAND,
  tt_NOT,
  tt_IF,
  tt_ELSE,
  tt_WHILE,
  tt_RETURN,
  tt_BREAK
};
void tokentype_to_string(char* s, enum lexer_tokentype tt);

struct lexer_token {
  enum lexer_tokentype tt;
  char lexeme[LEXER_LEXEME_SIZE];
  int lineno;
};

void lexer_init(struct Context* context);
void lexer_deinit(struct Context* context);
struct lexer_token* lexer_gettoken(struct Context* context);
struct lexer_token* lexer_peek(struct Context* context, int lookahead);

#endif
