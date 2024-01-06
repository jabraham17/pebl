#ifndef ARGUMENTS_H_
#define ARGUMENTS_H_
struct Arguments {
  char* inFilename;
  char* outFilename;
  int isDebug;
};

struct Arguments*
create_Arguments(char* inFilename, char* outFilename, int isDebug);

char* Arguments_inFilename(struct Arguments* args);
char* Arguments_outFilename(struct Arguments* args);
int Arguments_isDebug(struct Arguments* args);

#endif
