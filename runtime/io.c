#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

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
