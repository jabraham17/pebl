#include "parser/lexer.h"

#include "common/bsstring.h"

#include <ctype.h>
#include <errno.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <wctype.h>

#define MIN_LEXEME_SIZE 8

enum lexer_tokentype LT_type(struct lexer_token* t) { return t->tt; }
wchar_t* LT_lexeme(struct lexer_token* t) { return t->lexeme; }
int LT_lineno(struct lexer_token* t) { return t->lineno; }

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

static struct lexer_token* build_simple_token(
    struct Context* context,
    enum lexer_tokentype tt,
    wchar_t c) {
  struct lexer_token* t = empty_token(context);
  t->tt = tt;
  t->lexeme[0] = c;
  return t;
}
static struct lexer_token* build_simple_token2(
    struct Context* context,
    enum lexer_tokentype tt,
    wchar_t c1,
    wchar_t c2) {
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

static wchar_t get_char(struct Context* context) {
  return fgetwc(context->lexer->fp);
}
static int get_char_pos(struct Context* context, wchar_t* c) {
  int pos = ftell(context->lexer->fp);
  ASSERT_MSG(pos != -1, "ftell() failed - %s\n", strerror(errno));
  *c = get_char(context);
  return pos;
}
__attribute__((unused)) static wchar_t peek_char(struct Context* context) {
  wchar_t c;
  int pos = get_char_pos(context, &c);
  seek_pos(context, pos);
  return c;
}

static void skip_whitespace_comments(struct Context* context) {
  while(1) {
    wchar_t c;
    int pos = get_char_pos(context, &c);
    if(c == L'\n') {
      context->lexer->current_line++;
    } else if(iswspace(c)) {
      // continue
    } else if(c == L'#') {
      while(1) {
        c = get_char(context);
        if(c == L'\n' || c == L'\r' || c == EOF) {
          if(c == L'\n') {
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
  wchar_t literal;
  int pos = get_char_pos(context, &literal);
  if(literal == EOF) {
    // put the eof back and return error
    seek_pos(context, pos);
    return t;
  } else if(literal == L'\\') {
    // consume the '\'
    literal = get_char(context);
    // if the escape is a valid escape, keep it
    if(literal == L'\\') {
      literal = L'\\';
    } else if(literal == L'n') {
      literal = L'\n';
    } else if(literal == L't') {
      literal = L'\t';
    } else if(literal == L'0') {
      literal = L'\0';
    } else if(literal == L'\'') {
      literal = L'\'';
    } else {
      WARNING(context, "invalid escape sequence '\\%lc'\n", literal);
      return t;
    }
  }
  wchar_t next = get_char(context);
  if(next != L'\'') {
    return t;
  }
  t->lexeme[0] = literal;
  t->tt = tt_CHAR_LITERAL;
  return t;
}

// returns 0 if a charcter was read, 1 if its time to break, and -1 if an error
// occured.
static int handle_strings_get_char(struct Context* context, wchar_t* next) {
  int pos = get_char_pos(context, next);
  if(*next == EOF) {
    // put the eof back and return error
    seek_pos(context, pos);
    return -1;
  } else if(*next == L'\\') {
    // consume the '\'
    *next = get_char(context);
    // if the escape is a valid escape, keep it
    if(*next == L'\\') {
      *next = L'\\';
    } else if(*next == L'n') {
      *next = L'\n';
    } else if(*next == L't') {
      *next = L'\t';
    } else if(*next == L'0') {
      *next = L'\0';
    } else if(*next == L'"') {
      *next = L'"';
    } else {
      WARNING(context, "invalid escape sequence '\\%lc'\n", *next);
      return -1;
    }
  } else if(*next == L'"') {
    // consume the '"'
    return 1;
  }
  // keep going
  return 0;
}

// assumes we have already eaten the '"', just reads until the next '"'
static struct lexer_token* handle_strings(struct Context* context) {
  struct lexer_token* t = empty_token(context);

  int lexemeSize = MIN_LEXEME_SIZE;
  int nChars = 0;
  // read until we run out of chars or a space is encountered
  while(1) {
    wchar_t next;
    int res = handle_strings_get_char(context, &next);
    if(res == 1) break;
    else if(res == -1) return t; // ERROR

    if(nChars >= lexemeSize) {
      lexemeSize *= 2;
      t->lexeme = realloc(t->lexeme, sizeof(*t->lexeme) * lexemeSize);
    }
    t->lexeme[nChars] = next;
    nChars++;
  }
  // allocate just a bit more if needed for NUL
  if(nChars >= lexemeSize) {
    lexemeSize += 1;
    t->lexeme = realloc(t->lexeme, sizeof(*t->lexeme) * lexemeSize);
  }
  t->lexeme[nChars] = L'\0';

  // its a valid string literal
  t->tt = tt_STRING_LITERAL;
  return t;
}

static int is_valid_id(wchar_t* s) {
  // first char must be alnum or _
  if(!iswalpha(s[0]) && s[0] != L'_') return 0;

  int i = 1;
  while(s[i] != 0) {
    if(!iswalnum(s[i]) && s[i] != L'_') return 0;
    i++;
  }
  return i > 0;
}
static int is_valid_num(wchar_t* s) {
  int i = 0;
  while(s[i] != 0) {
    if(!iswdigit(s[i])) return 0;
    i++;
  }
  return i > 0;
}

static int is_keyword(wchar_t* s, wchar_t* keyword) {
  if(wcslen(s) != wcslen(keyword)) return 0;
  if(wcscmp(s, keyword) != 0) return 0;
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
  wchar_t c1;
  int pos1 = get_char_pos(context, &c1);
  switch(c1) {
    case L'(': return build_simple_token(context, tt_LPAREN, c1);
    case L')': return build_simple_token(context, tt_RPAREN, c1);
    case L'{': return build_simple_token(context, tt_LCURLY, c1);
    case L'}': return build_simple_token(context, tt_RCURLY, c1);
    case L',': return build_simple_token(context, tt_COMMA, c1);
    case L':': return build_simple_token(context, tt_COLON, c1);
    case L';': return build_simple_token(context, tt_SEMICOLON, c1);
    case L'+': return build_simple_token(context, tt_PLUS, c1);
    case L'*': return build_simple_token(context, tt_STAR, c1);
    case L'/': return build_simple_token(context, tt_DIVIDE, c1);
    case L'.': return build_simple_token(context, tt_DOT, c1);
  }

  // handle strings
  if(c1 == L'"') {
    return handle_strings(context);
  }

  // handle char
  if(c1 == L'\'') {
    return handle_char_literal(context);
  }

  // handle all 2 char tokens
  wchar_t c2;
  int pos2 = get_char_pos(context, &c2);

  if(c1 == L'-') {
    if(c2 == L'>') return build_simple_token2(context, tt_ARROW, c1, c2);
    else {
      seek_pos(context, pos2);
      return build_simple_token(context, tt_MINUS, c1);
    }
  } else if(c1 == L'&') {
    if(c2 == L'&') return build_simple_token2(context, tt_AND, c1, c2);
    else {
      seek_pos(context, pos2);
      return build_simple_token(context, tt_AMPERSAND, c1);
    }
  } else if(c1 == L'|') {
    if(c2 == L'|') return build_simple_token2(context, tt_OR, c1, c2);
    else {
      seek_pos(context, pos2);
      return build_simple_token(context, tt_ERROR, 0);
    }
  } else if(c1 == L'=') {
    if(c2 == L'=') return build_simple_token2(context, tt_EQ, c1, c2);
    else {
      seek_pos(context, pos2);
      return build_simple_token(context, tt_EQUALS, c1);
    }
  } else if(c1 == L'!') {
    if(c2 == L'=') return build_simple_token2(context, tt_NEQ, c1, c2);
    else {
      seek_pos(context, pos2);
      return build_simple_token(context, tt_NOT, c1);
    }
  } else if(c1 == L'<') {
    if(c2 == L'=') return build_simple_token2(context, tt_LTEQ, c1, c2);
    else {
      seek_pos(context, pos2);
      return build_simple_token(context, tt_LT, c1);
    }
  } else if(c1 == L'>') {
    if(c2 == L'=') return build_simple_token2(context, tt_GTEQ, c1, c2);
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
  wchar_t next;
  // read until we run out of chars or a non alphanumeric/underscore is found
  while(1) {
    int pos = get_char_pos(context, &next);
    if(!isalnum(next) && next != L'_') {
      seek_pos(context, pos);
      break;
    }
    if(next == EOF) break;

    if(nChars >= lexemeSize) {
      lexemeSize *= 2;
      t->lexeme = realloc(t->lexeme, sizeof(*t->lexeme) * lexemeSize);
    }
    t->lexeme[nChars] = next;
    nChars++;
  }
  if(next == EOF) {
    return eof_token(context);
  }
  // allocate just a bit more if needed for NUL
  if(nChars >= lexemeSize) {
    lexemeSize += 1;
    t->lexeme = realloc(t->lexeme, sizeof(*t->lexeme) * lexemeSize);
  }
  t->lexeme[nChars] = L'\0';

  wchar_t* tokenLexeme = LT_lexeme(t);
  if(is_keyword(tokenLexeme, L"if")) t->tt = tt_IF;
  else if(is_keyword(tokenLexeme, L"else")) t->tt = tt_ELSE;
  else if(is_keyword(tokenLexeme, L"while")) t->tt = tt_WHILE;
  else if(is_keyword(tokenLexeme, L"return")) t->tt = tt_RETURN;
  else if(is_keyword(tokenLexeme, L"break")) t->tt = tt_BREAK;
  else if(is_keyword(tokenLexeme, L"func")) t->tt = tt_FUNC;
  else if(is_keyword(tokenLexeme, L"extern")) t->tt = tt_EXTERN;
  else if(is_keyword(tokenLexeme, L"export")) t->tt = tt_EXPORT;
  else if(is_keyword(tokenLexeme, L"type")) t->tt = tt_TYPE;
  else if(is_keyword(tokenLexeme, L"let")) t->tt = tt_LET;
  else if(is_keyword(tokenLexeme, L"true")) t->tt = tt_TRUE;
  else if(is_keyword(tokenLexeme, L"false")) t->tt = tt_FALSE;
  // THESE MUST GO LAST
  else if(is_valid_id(tokenLexeme)) t->tt = tt_ID;
  else if(is_valid_num(tokenLexeme)) t->tt = tt_NUMBER;

  return t;
}

wchar_t* tokentype_to_string(enum lexer_tokentype tt) {
  if(tt == tt_EOF) return L"EOF";
  else if(tt == tt_ERROR) return L"ERROR";
  else if(tt == tt_ID) return L"ID";
  else if(tt == tt_NUMBER) return L"NUMBER";
  else if(tt == tt_TRUE) return L"TRUE";
  else if(tt == tt_FALSE) return L"FALSE";
  else if(tt == tt_STRING_LITERAL) return L"STRING_LITERAL";
  else if(tt == tt_CHAR_LITERAL) return L"CHAR_LITERAL";
  else if(tt == tt_LPAREN) return L"LPAREN";
  else if(tt == tt_RPAREN) return L"RPAREN";
  else if(tt == tt_LCURLY) return L"LCURLY";
  else if(tt == tt_RCURLY) return L"RCURLY";
  else if(tt == tt_COMMA) return L"COMMA";
  else if(tt == tt_COLON) return L"COLON";
  else if(tt == tt_SEMICOLON) return L"SEMICOLON";
  else if(tt == tt_FUNC) return L"FUNC";
  else if(tt == tt_EXTERN) return L"EXTERN";
  else if(tt == tt_TYPE) return L"TYPE";
  else if(tt == tt_LET) return L"LET";
  else if(tt == tt_PLUS) return L"PLUS";
  else if(tt == tt_MINUS) return L"MINUS";
  else if(tt == tt_STAR) return L"STAR";
  else if(tt == tt_DIVIDE) return L"DIVIDE";
  else if(tt == tt_AND) return L"AND";
  else if(tt == tt_OR) return L"OR";
  else if(tt == tt_LT) return L"LT";
  else if(tt == tt_GT) return L"GT";
  else if(tt == tt_LTEQ) return L"LTEQ";
  else if(tt == tt_GTEQ) return L"GTEQ";
  else if(tt == tt_EQ) return L"EQ";
  else if(tt == tt_NEQ) return L"NEQ";
  else if(tt == tt_EQUALS) return L"EQUALS";
  else if(tt == tt_AMPERSAND) return L"AMPERSAND";
  else if(tt == tt_NOT) return L"NOT";
  else if(tt == tt_IF) return L"IF";
  else if(tt == tt_ELSE) return L"ELSE";
  else if(tt == tt_WHILE) return L"WHILE";
  else if(tt == tt_RETURN) return L"RETURN";
  else if(tt == tt_BREAK) return L"BREAK";
  UNIMPLEMENTED("unknown token type");
}
