#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>


__attribute__((noreturn))
void pebl_panic(char* message) {
  fprintf(stderr, "%s\n", message);
  abort();
}

void* c_openFilePointer(int8_t* name, int8_t* mode) {
  return (void*)fopen((char*)name, (char*)mode);
}
void c_closeFilePointer(void* fp) {
  fclose((FILE*)fp);
}
int8_t c_getChar(void* fp) {
  return (int8_t)fgetc((FILE*)fp);
}
void c_putChar(void* fp, int8_t c) {
  fputc((char)c, (FILE*)fp);
}
void* c_getStdout() {
  return (void*)stdout;
}
void* c_getStdin() {
  return (void*)stdin;
}

int64_t c_ftell(void* fp) {
  return ftell((FILE*)fp);
}
void c_fseek(void* fp, int64_t offset, int64_t seek_type) {
  fseek((FILE*)fp, offset, seek_type);
}
int64_t c_fseek_seek_type(int64_t seek_type) {
  if (seek_type == 0) { return SEEK_SET; }
  else if (seek_type == 1) { return SEEK_CUR; }
  else if (seek_type == 2) { return SEEK_END; }
  else { pebl_panic("unknown seek type"); }
}

int64_t c_pathExists(int8_t* path) {
  return (access((char*)path, F_OK) == 0);
}
int64_t c_pathIsFile(int8_t* path) {
  struct stat path_stat;
  if(stat((char*)path, &path_stat) != 0) return 0;
  return S_ISREG(path_stat.st_mode);
}
