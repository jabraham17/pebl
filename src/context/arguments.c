
#include "context/arguments.h"

#include "common/bsstring.h"
#include "context/context.h"

#include <stdlib.h>

struct Arguments*
create_Arguments(char* inFilename, char* outFilename, int isDebug) {
  struct Arguments* args = malloc(sizeof(*args));

  args->inFilename = inFilename ? bsstrdup(inFilename) : NULL;
  args->outFilename = outFilename ? bsstrdup(outFilename) : NULL;
  args->isDebug = isDebug;

  return args;
}

char* Arguments_inFilename(struct Arguments* args) {
  ASSERT(args && args->inFilename);
  return args->inFilename;
}
char* Arguments_outFilename(struct Arguments* args) {
  ASSERT(args && args->outFilename);
  return args->outFilename;
}
int Arguments_isDebug(struct Arguments* args) {
  ASSERT(args);
  return args->isDebug;
}
