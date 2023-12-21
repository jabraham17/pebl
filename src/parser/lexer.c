#include "parser/lexer.h"

#include "common/bsstring.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

void lexer_init(struct Context* context, FILE* fp) {
  context->lexer = malloc(sizeof(*context->lexer));
  memset(context->lexer, 0, sizeof(*context->lexer));
  context->lexer->fp = fp;
  context->lexer->current_line = 1;
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
  t->lineno = context->lexer->current_line;
  memset(t->lexeme, 0, LEXER_LEXEME_SIZE);
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
static char peek_char(struct Context* context) {
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

// assumes we have already eaten the '"', just reads until the next '"'
static struct lexer_token* handle_strings(struct Context* context) {
  struct lexer_token* t = empty_token(context);
  int nChars = 0;
  // read until we run out of chars or a space is encountered
  while(nChars < LEXER_LEXEME_SIZE) {
    char next;
    int pos = get_char_pos(context, &next);
    if(next == EOF) {
      // put the eof back and return error
      seek_pos(context, pos);
      return t;
    }
    if(next == '"') {
      // consume the '"'
      break;
    }
    t->lexeme[nChars] = next;
    nChars++;
  }
  if(nChars == LEXER_LEXEME_SIZE) {
    return t; // error, token was too big
  }

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
  int nChars = 0;
  // read until we run out of chars or a non alphanumeric/underscore is found
  char next;
  while(nChars < LEXER_LEXEME_SIZE) {
    int pos = get_char_pos(context, &next);
    if(!isalnum(next) && next != '_') {
      seek_pos(context, pos);
      break;
    }
    if(next == EOF) break;
    t->lexeme[nChars] = next;
    nChars++;
  }
  if(nChars == LEXER_LEXEME_SIZE) {
    return empty_token(context); // error, token was too big
  }
  if(next == EOF) {
    return eof_token(context);
  }

  if(is_keyword(t->lexeme, "if")) t->tt = tt_IF;
  else if(is_keyword(t->lexeme, "else")) t->tt = tt_ELSE;
  else if(is_keyword(t->lexeme, "while")) t->tt = tt_WHILE;
  else if(is_keyword(t->lexeme, "return")) t->tt = tt_RETURN;
  else if(is_keyword(t->lexeme, "break")) t->tt = tt_BREAK;
  else if(is_keyword(t->lexeme, "func")) t->tt = tt_FUNC;
  else if(is_keyword(t->lexeme, "extern")) t->tt = tt_EXTERN;
  else if(is_keyword(t->lexeme, "export")) t->tt = tt_EXPORT;
  else if(is_keyword(t->lexeme, "type")) t->tt = tt_TYPE;
  else if(is_keyword(t->lexeme, "let")) t->tt = tt_LET;
  else if(is_valid_id(t->lexeme)) t->tt = tt_ID;
  else if(is_valid_num(t->lexeme)) t->tt = tt_NUMBER;

  return t;
}

void tokentype_to_string(char* s, enum lexer_tokentype tt) {
  if(tt == tt_EOF) bsstrcpy(s, "EOF");
  else if(tt == tt_ERROR) bsstrcpy(s, "ERROR");
  else if(tt == tt_ID) bsstrcpy(s, "ID");
  else if(tt == tt_NUMBER) bsstrcpy(s, "NUMBER");
  else if(tt == tt_STRING_LITERAL) bsstrcpy(s, "STRING_LITERAL");
  else if(tt == tt_LPAREN) bsstrcpy(s, "LPAREN");
  else if(tt == tt_RPAREN) bsstrcpy(s, "RPAREN");
  else if(tt == tt_LCURLY) bsstrcpy(s, "LCURLY");
  else if(tt == tt_RCURLY) bsstrcpy(s, "RCURLY");
  else if(tt == tt_COMMA) bsstrcpy(s, "COMMA");
  else if(tt == tt_COLON) bsstrcpy(s, "COLON");
  else if(tt == tt_SEMICOLON) bsstrcpy(s, "SEMICOLON");
  else if(tt == tt_FUNC) bsstrcpy(s, "FUNC");
  else if(tt == tt_EXTERN) bsstrcpy(s, "EXTERN");
  else if(tt == tt_TYPE) bsstrcpy(s, "TYPE");
  else if(tt == tt_LET) bsstrcpy(s, "LET");
  else if(tt == tt_PLUS) bsstrcpy(s, "PLUS");
  else if(tt == tt_MINUS) bsstrcpy(s, "MINUS");
  else if(tt == tt_STAR) bsstrcpy(s, "STAR");
  else if(tt == tt_DIVIDE) bsstrcpy(s, "DIVIDE");
  else if(tt == tt_AND) bsstrcpy(s, "AND");
  else if(tt == tt_OR) bsstrcpy(s, "OR");
  else if(tt == tt_LT) bsstrcpy(s, "LT");
  else if(tt == tt_GT) bsstrcpy(s, "GT");
  else if(tt == tt_LTEQ) bsstrcpy(s, "LTEQ");
  else if(tt == tt_GTEQ) bsstrcpy(s, "GTEQ");
  else if(tt == tt_EQ) bsstrcpy(s, "EQ");
  else if(tt == tt_NEQ) bsstrcpy(s, "NEQ");
  else if(tt == tt_EQUALS) bsstrcpy(s, "EQUALS");
  else if(tt == tt_AMPERSAND) bsstrcpy(s, "AMPERSAND");
  else if(tt == tt_NOT) bsstrcpy(s, "NOT");
  else if(tt == tt_IF) bsstrcpy(s, "IF");
  else if(tt == tt_ELSE) bsstrcpy(s, "ELSE");
  else if(tt == tt_WHILE) bsstrcpy(s, "WHILE");
  else if(tt == tt_RETURN) bsstrcpy(s, "RETURN");
  else if(tt == tt_BREAK) bsstrcpy(s, "BREAK");
}
