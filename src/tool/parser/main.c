

#include "ast/TypeTable.h"
#include "ast/parse-checks.h"
#include "context/context.h"
#include "parser/parser.h"

#include <string.h>

void do_scope(struct Context* c);

int main(int argc, char** argv) {

  char* filename = NULL;
  int print = 0;
  int verify = 1;
  int checks = 1;
  int scope = 0;

  int i = 1;
  while(i < argc) {
    char* arg = argv[i];
    if(arg[0] != '-') {
      // if already have filename, warn
      if(filename) {
        fprintf(
            stderr,
            "Warning: multiple files specifed, ignore '%s'\n",
            filename);
      }
      filename = argv[i];
    } else {
      char* flag = arg + 1;
      int val_to_set = 1;
      // if -no-, set val_to_set to 0
      if(flag[0] == 'n' && flag[1] == 'o' && flag[2] == '-') {
        val_to_set = 0;
        flag = flag + 3;
      }
      if(strcmp(flag, "print") == 0) {
        print = val_to_set;
      } else if(strcmp(flag, "verify") == 0) {
        verify = val_to_set;
      } else if(strcmp(flag, "checks") == 0) {
        checks = val_to_set;
      } else if(strcmp(flag, "scope") == 0) {
        scope = val_to_set;
      } else {
        fprintf(stderr, "Warning: unknown flag '%s'\n", arg);
      }
    }

    i++;
  }

  if(filename == NULL) {
    fprintf(
        stderr,
        "Error - usage: './parser <filename> (-no-print)? (-verify)? "
        "(-checks)?'\n");
    return 1;
  }

  struct Context context_;
  struct Context* context = &context_;
  Context_init(context, filename);
  lexer_init(context);
  parser_init(context);
  parser_parse(context);
  lexer_deinit(context);

  if(verify) verify_ast(context);
  if(checks) parse_checks(context);
  if(print) dump_ast(context);

  // build types
  // Context_install_builtin_types(context);
  // TypeTable_add_from_ast(context, context->ast);

  if(scope) do_scope(context);

  return 0;
}
