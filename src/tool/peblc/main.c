

#include "ast/parse-checks.h"
#include "ast/scope-resolve.h"
#include "builtins/compiler-builtin.h"
#include "codegen/codegen-llvm.h"
#include "common/bsstring.h"
#include "context/arguments.h"
#include "context/context.h"
#include "parser/parser.h"

#include <string.h>

int main(int argc, char** argv) {

  char* filename = NULL;
  int verify = 1;
  int checks = 1;
  char* outfile = NULL;
  int debug = 0;

  int i = 1;
  while(i < argc) {
    char* arg = argv[i];
    if(arg[0] != '-') {
      // if already have filename, warn
      if(filename) {
        fwprintf(
            stderr,
            L"Warning: multiple files specifed, ignore '%s'\n",
            filename);
      }
      filename = arg;
    } else {
      char* flag = arg + 1;
      int val_to_set = 1;
      // if -no-, set val_to_set to 0
      if(flag[0] == 'n' && flag[1] == 'o' && flag[2] == '-') {
        val_to_set = 0;
        flag = flag + 3;
      }

      if(strcmp(flag, "verify") == 0) {
        verify = val_to_set;
      } else if(strcmp(flag, "checks") == 0) {
        checks = val_to_set;
      } else if(strcmp(flag, "output") == 0) {
        i++;
        outfile = argv[i];
      } else if(strcmp(flag, "g") == 0) {
        debug = val_to_set;
      } else {
        fwprintf(stderr, L"Warning: unknown flag '%s'\n", arg);
      }
    }

    i++;
  }

  if(filename == NULL) {
    fwprintf(
        stderr,
        L"Error - usage: './peblc <filename> (-output FILENAME)? (-checks?) "
        "-(verify)? (-g)?'\n");
    return 1;
  }
  if(outfile == NULL) {
    outfile = bsstrcat(filename, ".ll");
  }

  struct Context context_;
  struct Context* context = &context_;
  struct Arguments* args = create_Arguments(filename, outfile, debug);
  Context_init(context, args);
  lexer_init(context);
  parser_init(context);

  parser_parse(context);
  lexer_deinit(context);

  if(verify) verify_ast(context);
  if(checks) parse_checks(context);

  scope_resolve(context);

  init_cg_context(context);
  codegen(context);
  cg_emit(context);
  deinit_cg_context(context);

  return 0;
}
