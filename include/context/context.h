
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
  struct lexer_state* lexer;

  struct AstNode* ast;
  struct Location* locations;

  struct Type* types;
  struct ScopeResult* scope_table;

  struct CompilerBuiltin* compiler_builtins;

  struct cg_context* codegen;
};

struct Context* Context_allocate();
void Context_init(struct Context* context);

#define WARNING_ON_AST(context, ast, format, ...)                              \
  do {                                                                         \
    struct Location* loc = Context_get_location(context, ast);                 \
    if(loc) {                                                                  \
      fprintf(                                                                 \
          stderr,                                                              \
          "line %d: warning: " format,                                         \
          loc->line_start,                                                     \
          ##__VA_ARGS__);                                                      \
    } else {                                                                   \
      fprintf(stderr, "<unknown>: warning: " format, ##__VA_ARGS__);           \
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

#define ERROR_ON_AST(context, ast, format, ...)                                \
  do {                                                                         \
    struct Location* loc = Context_get_location(context, ast);                 \
    if(loc) {                                                                  \
      fprintf(                                                                 \
          stderr,                                                              \
          "line %d: error: " format,                                           \
          loc->line_start,                                                     \
          ##__VA_ARGS__);                                                      \
    } else {                                                                   \
      fprintf(stderr, "<unknown>: error: " format, ##__VA_ARGS__);             \
    }                                                                          \
    FATAL_EXIT();                                                              \
  } while(0)

#define ERROR(context, format, ...)                                            \
  do {                                                                         \
    ERROR_ON_AST(context, NULL, format, ##__VA_ARGS__);                        \
  } while(0)

#endif
