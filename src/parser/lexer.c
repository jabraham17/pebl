#include "parser/lexer.h"

#include "common/bsstring.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#define MIN_LEXEME_SIZE 8

enum lexer_tokentype LT_type(struct lexer_token* t) {
  return t->tt;
}
char* LT_lexeme(struct lexer_token* t) {
  return t->lexeme;
}
int LT_lineno(struct lexer_token* t) {
  return t->lineno;
}

void lexer_init(struct Context* context) {
  context->lexer = malloc(sizeof(*context->lexer));
  memset(context->lexer, 0, sizeof(*context->lexer));

  context->lexer->fp = fopen(context->filename, "rb");
  if(context->lexer->fp == NULL) {
    ERROR(context, "could not open file named '%s'\n", context->filename);
  }
  context->lexer->current_line = 1;
}
void lexer_deinit(struct Context* context) {
  fclose(context->lexer->fp);
  free(context->lexer);
}
static struct lexer_token* lexer_gettoken_internal(struct Context* context);
struct lexer_token* lexer_peek(struct Context* context, int lookahead) {
  ASSERT(lookahead >= 0 && lookahead <= 2);

  if(context->lexer->peeked[lookahead - 1])
    return context->lexer->peeked[lookahead - 1];
  else {
    if(lookahead >= 1 && !context->lexer->peeked[0]) {
      context->lexer->peeked[0] = lexer_gettoken_internal(context);
    }
    if(lookahead >= 2 && !context->lexer->peeked[1]) {
      context->lexer->peeked[1] = lexer_gettoken_internal(context);
    }
    return context->lexer->peeked[lookahead - 1];
  }
}

static struct lexer_token* empty_token(struct Context* context) {
  struct lexer_token* t = malloc(sizeof(*t));
  t->tt = tt_ERROR;
  t->lexeme = calloc(MIN_LEXEME_SIZE, sizeof(*t->lexeme));
  t->lineno = context->lexer->current_line;
  return t;
}
static struct lexer_token* eof_token(struct Context* context) {
  struct lexer_token* t = empty_token(context);
  t->tt = tt_EOF;
  return t;
}

static struct lexer_token*
build_simple_token(struct Context* context, enum lexer_tokentype tt, char c) {
  struct lexer_token* t = empty_token(context);
  t->tt = tt;
  t->lexeme[0] = c;
  return t;
}
static struct lexer_token* build_simple_token2(
    struct Context* context,
    enum lexer_tokentype tt,
    char c1,
    char c2) {
  struct lexer_token* t = empty_token(context);
  t->tt = tt;
  t->lexeme[0] = c1;
  t->lexeme[1] = c2;
  return t;
}
static void seek_pos(struct Context* context, int pos) {
  // we have to use the tell pos and not just go -1 on SEEK_CUR due to stupid
  // windows
  int res = fseek(context->lexer->fp, pos, SEEK_SET);
  ASSERT_MSG(!res, "fseek() failed - %s\n", strerror(errno));
}

static char get_char(struct Context* context) {
  return fgetc(context->lexer->fp);
}
static int get_char_pos(struct Context* context, char* c) {
  int pos = ftell(context->lexer->fp);
  ASSERT_MSG(pos != -1, "ftell() failed - %s\n", strerror(errno));
  *c = get_char(context);
  return pos;
}
__attribute__((unused)) static char peek_char(struct Context* context) {
  char c;
  int pos = get_char_pos(context, &c);
  seek_pos(context, pos);
  return c;
}

static void skip_whitespace_comments(struct Context* context) {
  while(1) {
    char c;
    int pos = get_char_pos(context, &c);
    if(c == '\n') {
      context->lexer->current_line++;
    } else if(isspace(c)) {
      // continue
    } else if(c == '#') {
      while(1) {
        c = get_char(context);
        if(c == '\n' || c == '\r' || c == EOF) {
          if(c == '\n') {
            context->lexer->current_line++;
          }
          break;
        }
      }
    } else {
      seek_pos(context, pos);
      break;
    }
  }
}

// assumes we have already eaten the ', just reads until the next '
static struct lexer_token* handle_char_literal(struct Context* context) {
  struct lexer_token* t = empty_token(context);
  char literal;
  int pos = get_char_pos(context, &literal);
  if(literal == EOF) {
    // put the eof back and return error
    seek_pos(context, pos);
    return t;
  }
  else if(literal == '\\') {
    // consume the '\'
    literal = get_char(context);
    // if the escape is a valid escape, keep it
    if(literal == '\\') {
      literal = '\\';
    } else if(literal == 'n') {
      literal = '\n';
    } else if(literal == 't') {
      literal = '\t';
    } else if(literal == '0') {
      literal = '\0';
    } else if(literal == '\'') {
      literal = '\'';
    } else {
      WARNING(context, "invalid escape sequence '\\%c'\n", literal);
      return t;
    }
  }
  char next = get_char(context);
  if(next != '\'') {
    return t;
  }
  t->lexeme[0] = literal;
  t->tt = tt_CHAR_LITERAL;
  return t;
}

// returns 0 if a charcter was read, 1 if its time to break, and -1 if an error occured.
static int handle_strings_get_char(struct Context* context, char* next) {
    int pos = get_char_pos(context, next);
    if(*next == EOF) {
      // put the eof back and return error
      seek_pos(context, pos);
      return -1;
    }
    else if(*next == '\\') {
      // consume the '\'
      *next = get_char(context);
      // if the escape is a valid escape, keep it
      if(*next == '\\') {
        *next = '\\';
      } else if(*next == 'n') {
        *next = '\n';
      } else if(*next == 't') {
        *next = '\t';
      } else if(*next == '0') {
        *next = '\0';
      } else if(*next == '"') {
        *next = '"';
      } else {
        WARNING(context, "invalid escape sequence '\\%c'\n", *next);
        return -1;
      }
    }
    else if(*next == '"') {
      // consume the '"'
      return 1;
    }
    //keep going
    return 0;
}

// assumes we have already eaten the '"', just reads until the next '"'
static struct lexer_token* handle_strings(struct Context* context) {
  struct lexer_token* t = empty_token(context);

  int lexemeSize = MIN_LEXEME_SIZE;
  int nChars = 0;
  // read until we run out of chars or a space is encountered
  while(1) {
    char next;
    int res = handle_strings_get_char(context, &next);
    if (res == 1) break;
    else if(res == -1) return t; // ERROR

    if (nChars >= lexemeSize) {
      lexemeSize *= 2;
      t->lexeme = realloc(t->lexeme, lexemeSize);
    }
    t->lexeme[nChars] = next;
    nChars++;
  }
  // allocate just a bit more if needed for NUL
  if (nChars >= lexemeSize) {
    lexemeSize += 1;
    t->lexeme = realloc(t->lexeme, lexemeSize);
  }
  t->lexeme[nChars] = '\0';

  // its a valid string literal
  t->tt = tt_STRING_LITERAL;
  return t;
}

static int is_valid_id(char* s) {
  // first char must be alnum or _
  if(!isalpha(s[0]) && s[0] != '_') return 0;

  int i = 1;
  while(s[i] != 0) {
    if(!isalnum(s[i]) && s[i] != '_') return 0;
    i++;
  }
  return i > 0;
}
static int is_valid_num(char* s) {
  int i = 0;
  while(s[i] != 0) {
    if(!isdigit(s[i])) return 0;
    i++;
  }
  return i > 0;
}

static int is_keyword(char* s, char* keyword) {
  if(strlen(s) != strlen(keyword)) return 0;
  if(strcmp(s, keyword) != 0) return 0;
  return 1;
}

struct lexer_token* lexer_gettoken(struct Context* context) {

  if(context->lexer->peeked[0]) {
    struct lexer_token* ret = context->lexer->peeked[0];
    context->lexer->peeked[0] = context->lexer->peeked[1];
    context->lexer->peeked[1] = NULL;
    return ret;
  }
  return lexer_gettoken_internal(context);
}
static struct lexer_token* lexer_gettoken_internal(struct Context* context) {

  skip_whitespace_comments(context);
  // handle all single char tokens
  char c1;
  int pos1 = get_char_pos(context, &c1);
  switch(c1) {
    case '(': return build_simple_token(context, tt_LPAREN, c1);
    case ')': return build_simple_token(context, tt_RPAREN, c1);
    case '{': return build_simple_token(context, tt_LCURLY, c1);
    case '}': return build_simple_token(context, tt_RCURLY, c1);
    case ',': return build_simple_token(context, tt_COMMA, c1);
    case ':': return build_simple_token(context, tt_COLON, c1);
    case ';': return build_simple_token(context, tt_SEMICOLON, c1);
    case '+': return build_simple_token(context, tt_PLUS, c1);
    case '*': return build_simple_token(context, tt_STAR, c1);
    case '/': return build_simple_token(context, tt_DIVIDE, c1);
    case '.': return build_simple_token(context, tt_DOT, c1);
  }

  // handle strings
  if(c1 == '"') {
    return handle_strings(context);
  }

  // handle char
  if(c1 == '\'') {
    return handle_char_literal(context);
  }

  // handle all 2 char tokens
  char c2;
  int pos2 = get_char_pos(context, &c2);

  if(c1 == '-') {
    if(c2 == '>') return build_simple_token2(context, tt_ARROW, c1, c2);
    else {
      seek_pos(context, pos2);
      return build_simple_token(context, tt_MINUS, c1);
    }
  } else if(c1 == '&') {
    if(c2 == '&') return build_simple_token2(context, tt_AND, c1, c2);
    else {
      seek_pos(context, pos2);
      return build_simple_token(context, tt_AMPERSAND, c1);
    }
  } else if(c1 == '|') {
    if(c2 == '|') return build_simple_token2(context, tt_OR, c1, c2);
    else {
      seek_pos(context, pos2);
      return build_simple_token(context, tt_ERROR, 0);
    }
  } else if(c1 == '=') {
    if(c2 == '=') return build_simple_token2(context, tt_EQ, c1, c2);
    else {
      seek_pos(context, pos2);
      return build_simple_token(context, tt_EQUALS, c1);
    }
  } else if(c1 == '!') {
    if(c2 == '=') return build_simple_token2(context, tt_NEQ, c1, c2);
    else {
      seek_pos(context, pos2);
      return build_simple_token(context, tt_NOT, c1);
    }
  } else if(c1 == '<') {
    if(c2 == '=') return build_simple_token2(context, tt_LTEQ, c1, c2);
    else {
      seek_pos(context, pos2);
      return build_simple_token(context, tt_LT, c1);
    }
  } else if(c1 == '>') {
    if(c2 == '=') return build_simple_token2(context, tt_GTEQ, c1, c2);
    else {
      seek_pos(context, pos2);
      return build_simple_token(context, tt_GT, c1);
    }
  }

  // handle the general case
  // its simpler to just go back two chars
  seek_pos(context, pos1);

  struct lexer_token* t = empty_token(context);
  int lexemeSize = MIN_LEXEME_SIZE;
  int nChars = 0;
  char next;
  // read until we run out of chars or a non alphanumeric/underscore is found
  while(1) {
    int pos = get_char_pos(context, &next);
    if(!isalnum(next) && next != '_') {
      seek_pos(context, pos);
      break;
    }
    if(next == EOF) break;

    if (nChars >= lexemeSize) {
      lexemeSize *= 2;
      t->lexeme = realloc(t->lexeme, lexemeSize);
    }
    t->lexeme[nChars] = next;
    nChars++;
  }
  if(next == EOF) {
    return eof_token(context);
  }
  // allocate just a bit more if needed for NUL
  if (nChars >= lexemeSize) {
    lexemeSize += 1;
    t->lexeme = realloc(t->lexeme, lexemeSize);
  }
  t->lexeme[nChars] = '\0';

  char* tokenLexeme = LT_lexeme(t);
  if(is_keyword(tokenLexeme, "if")) t->tt = tt_IF;
  else if(is_keyword(tokenLexeme, "else")) t->tt = tt_ELSE;
  else if(is_keyword(tokenLexeme, "while")) t->tt = tt_WHILE;
  else if(is_keyword(tokenLexeme, "return")) t->tt = tt_RETURN;
  else if(is_keyword(tokenLexeme, "break")) t->tt = tt_BREAK;
  else if(is_keyword(tokenLexeme, "func")) t->tt = tt_FUNC;
  else if(is_keyword(tokenLexeme, "extern")) t->tt = tt_EXTERN;
  else if(is_keyword(tokenLexeme, "export")) t->tt = tt_EXPORT;
  else if(is_keyword(tokenLexeme, "type")) t->tt = tt_TYPE;
  else if(is_keyword(tokenLexeme, "let")) t->tt = tt_LET;
  else if(is_keyword(tokenLexeme, "true")) t->tt = tt_TRUE;
  else if(is_keyword(tokenLexeme, "false")) t->tt = tt_FALSE;
  // THESE MUST GO LAST
  else if(is_valid_id(tokenLexeme)) t->tt = tt_ID;
  else if(is_valid_num(tokenLexeme)) t->tt = tt_NUMBER;

  return t;
}

char* tokentype_to_string(enum lexer_tokentype tt) {
  if(tt == tt_EOF) return "EOF";
  else if(tt == tt_ERROR) return "ERROR";
  else if(tt == tt_ID) return "ID";
  else if(tt == tt_NUMBER) return "NUMBER";
  else if(tt == tt_TRUE) return "TRUE";
  else if(tt == tt_FALSE) return "FALSE";
  else if(tt == tt_STRING_LITERAL) return "STRING_LITERAL";
  else if(tt == tt_CHAR_LITERAL) return "CHAR_LITERAL";
  else if(tt == tt_LPAREN) return "LPAREN";
  else if(tt == tt_RPAREN) return "RPAREN";
  else if(tt == tt_LCURLY) return "LCURLY";
  else if(tt == tt_RCURLY) return "RCURLY";
  else if(tt == tt_COMMA) return "COMMA";
  else if(tt == tt_COLON) return "COLON";
  else if(tt == tt_SEMICOLON) return "SEMICOLON";
  else if(tt == tt_FUNC) return "FUNC";
  else if(tt == tt_EXTERN) return "EXTERN";
  else if(tt == tt_TYPE) return "TYPE";
  else if(tt == tt_LET) return "LET";
  else if(tt == tt_PLUS) return "PLUS";
  else if(tt == tt_MINUS) return "MINUS";
  else if(tt == tt_STAR) return "STAR";
  else if(tt == tt_DIVIDE) return "DIVIDE";
  else if(tt == tt_AND) return "AND";
  else if(tt == tt_OR) return "OR";
  else if(tt == tt_LT) return "LT";
  else if(tt == tt_GT) return "GT";
  else if(tt == tt_LTEQ) return "LTEQ";
  else if(tt == tt_GTEQ) return "GTEQ";
  else if(tt == tt_EQ) return "EQ";
  else if(tt == tt_NEQ) return "NEQ";
  else if(tt == tt_EQUALS) return "EQUALS";
  else if(tt == tt_AMPERSAND) return "AMPERSAND";
  else if(tt == tt_NOT) return "NOT";
  else if(tt == tt_IF) return "IF";
  else if(tt == tt_ELSE) return "ELSE";
  else if(tt == tt_WHILE) return "WHILE";
  else if(tt == tt_RETURN) return "RETURN";
  else if(tt == tt_BREAK) return "BREAK";
  UNIMPLEMENTED("unknown token type");
  return "";
}
