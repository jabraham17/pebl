

#include "ast/parse-checks.h"
#include "ast/scope-resolve.h"
#include "builtins/compiler-builtin.h"
#include "codegen/codegen-llvm.h"
#include "common/bsstring.h"
#include "context/context.h"
#include "parser/parser.h"

#include <string.h>

int main(int argc, char** argv) {

  char* filename = NULL;
  int verify = 1;
  int checks = 1;
  char* outfile = NULL;

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
      } else {
        fprintf(stderr, "Warning: unknown flag '%s'\n", arg);
      }
    }

    i++;
  }

  if(filename == NULL) {
    fprintf(
        stderr,
        "Error - usage: './peblc <filename> (-output FILENAME)? (-checks?) "
        "-(verify)?'\n");
    return 1;
  }
  if(outfile == NULL) {
    outfile = bsstrcat(filename, ".ll");
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

  register_compiler_builtins(context);

  scope_resolve(context);

  init_cg_context(context, outfile);
  codegen(context);
  cg_emit(context);

  return 0;
}
