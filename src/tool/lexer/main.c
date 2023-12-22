

#include "context/context.h"
#include "parser/parser.h"

void print_token(struct lexer_token* t) {
  char buf[32];
  tokentype_to_string(buf, t->tt);
  printf("{%s, lexeme='%s'}\n", buf, t->lexeme);
}

int main(int argc, char** argv) {

  if(argc < 2) {
    printf("Error - usage: './lexer <filename>'\n");
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
    if(t->tt == tt_EOF || t->tt == tt_ERROR) break;
  }

  lexer_deinit(context);

  return 0;
}
