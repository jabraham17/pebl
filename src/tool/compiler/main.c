

#include "ast/TypeTable.h"
#include "ast/parse-checks.h"
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
        "Error - usage: './parser <filename> (-verify)? (-checks)?'\n");
    return 1;
  }
  if(outfile == NULL) {
    outfile = bsstrcat(filename, ".ll");
  }

  FILE* fp = fopen(filename, "rb");
  if(fp == NULL) {
    fprintf(stderr, "Error - could not open file\n");
    return 1;
  }

  struct Context context_;
  struct Context* context = &context_;
  Context_init(context);
  lexer_init(context, fp);
  parser_init(context);

  parser_parse(context);

  fclose(fp);

  if(verify) verify_ast(context);
  if(checks) parse_checks(context);

  Context_install_builtin_types(context);
  TypeTable_add_from_ast(context, context->ast);

  register_compiler_builtins(context);

  init_cg_context(context, outfile);
  codegen(context);
  cg_emit(context);

  return 0;
}
