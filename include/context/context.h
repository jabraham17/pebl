
#ifndef CONTEXT_H_
#define CONTEXT_H_

#include "ast/location.h"

#include <stdio.h>

struct lexer_state;
struct AstNode;
struct Location;
struct Type;
struct ScopeResult;
struct cg_context;
struct CompilerBuiltin;

struct Context {
  char* filename;
  struct lexer_state* lexer;

  struct AstNode* ast;
  struct Location* locations;

  struct ScopeResult* scope_table;

  struct CompilerBuiltin* compiler_builtins;

  struct cg_context* codegen;
};

struct Context* Context_allocate();
void Context_init(struct Context* context, char* filename);

#define WARNING_ON_AST(context, ast, format, ...)                              \
  do {                                                                         \
    struct Location* loc = Context_get_location(context, ast);                 \
    if(loc) {                                                                  \
      fprintf(                                                                 \
          stderr,                                                              \
          "%s:%d: warning: " format,                                           \
          context->filename,                                                   \
          loc->line_start,                                                     \
          ##__VA_ARGS__);                                                      \
    } else {                                                                   \
      fprintf(                                                                 \
          stderr,                                                              \
          "%s: warning: " format,                                              \
          context->filename,                                                   \
          ##__VA_ARGS__);                                                      \
    }                                                                          \
  } while(0)

#define WARNING(context, format, ...)                                          \
  do {                                                                         \
    WARNING_ON_AST(context, NULL, format, ##__VA_ARGS__);                      \
  } while(0)

#define FATAL_EXIT() exit(1)

#define ASSERT_MSG(cond, format, ...)                                          \
  do {                                                                         \
    if(!(cond)) {                                                              \
      fprintf(stderr, "Assert failed: " format, ##__VA_ARGS__);                \
      FATAL_EXIT();                                                            \
    }                                                                          \
  } while(0)
#define ASSERT(cond) ASSERT_MSG(cond, #cond "\n")

#define UNIMPLEMENTED(format, ...)                                             \
  do {                                                                         \
    fprintf(stderr, "Unimplemented: " format, ##__VA_ARGS__);                  \
    FATAL_EXIT();                                                              \
  } while(0)

#define ERROR_ON_LINE(context, lineno, format, ...)                            \
  do {                                                                         \
    fprintf(                                                                   \
        stderr,                                                                \
        "%s:%d: error: " format,                                               \
        context->filename,                                                     \
        lineno,                                                                \
        ##__VA_ARGS__);                                                        \
    FATAL_EXIT();                                                              \
  } while(0)

#define ERROR_ON_AST(context, ast, format, ...)                                \
  do {                                                                         \
    struct Location* loc = Context_get_location(context, ast);                 \
    if(loc) {                                                                  \
      ERROR_ON_LINE(context, loc->line_start, format, ##__VA_ARGS__);          \
    } else {                                                                   \
      fprintf(stderr, "%s: error: " format, context->filename, ##__VA_ARGS__); \
    }                                                                          \
    FATAL_EXIT();                                                              \
  } while(0)

#define ERROR(context, format, ...)                                            \
  do {                                                                         \
    ERROR_ON_AST(context, NULL, format, ##__VA_ARGS__);                        \
  } while(0)

#endif
