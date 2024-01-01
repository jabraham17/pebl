

#include "context/context.h"
#include "parser/parser.h"

void print_token(struct lexer_token* t) {
  wprintf(
      L"{%ls, lexeme='%s'}\n",
      tokentype_to_string(LT_type(t)),
      LT_lexeme(t));
}

int main(int argc, char** argv) {

  if(argc < 2) {
    fwprintf(stderr, L"Error - usage: './lexer <filename>'\n");
    return 1;
  }

  char* filename = argv[1];

  struct Context context_;
  struct Context* context = &context_;
  Context_init(context, filename);
  lexer_init(context);

  struct lexer_token* t;
  while(1) {
    t = lexer_gettoken(context);
    print_token(t);
    if(LT_type(t) == tt_EOF || LT_type(t) == tt_ERROR) break;
  }

  lexer_deinit(context);

  return 0;
}
