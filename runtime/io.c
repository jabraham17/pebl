#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <wchar.h>
#include <string.h>


void* c_allocate(int64_t n);

__attribute__((noreturn))
void pebl_panic(char* message) {
  fwprintf(stderr, L"%s\n", message);
  abort();
}
int c_is_char_only(wchar_t* s) {
  while (*s) {
    if (*s > 255) return 0;
    s++;
  }
  return 1;
}
// assumes char only
char* c_to_char_only(wchar_t* s) {
  int len = wcslen(s);
  char* only_chars = c_allocate(sizeof(*only_chars)*(len+1));
  only_chars[len] = '\0';
  for (int i = 0; i < len; i++) {
    only_chars[i] = (char)s[i];
  }
  return only_chars;
}
wchar_t* c_to_wchar(char* s) {
  int len = strlen(s);
  wchar_t* wcs = c_allocate(sizeof(*wcs)*(len+1));
  wcs[len] = '\0';
  for (int i = 0; i < len; i++) {
    wcs[i] = (wchar_t)s[i];
  }
  return wcs;
}


void* c_openFilePointer(wchar_t* name, wchar_t* mode) {
  if(!c_is_char_only(name) || !c_is_char_only(mode)) {
    pebl_panic("only ascii characters allowed");
  }
  return (void*)fopen(c_to_char_only(name), c_to_char_only(mode));
}
void c_closeFilePointer(void* fp) {
  fclose((FILE*)fp);
}
wchar_t c_getChar(void* fp) {
  return fgetwc((FILE*)fp);
}
void c_putChar(void* fp, wchar_t c) {
  fputwc(c, (FILE*)fp);
}
void c_putWChar(void* fp, wchar_t c) {
  fputwc(c, (FILE*)fp);
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

int64_t c_pathExists(wchar_t* path) {
  if(c_is_char_only(path)) {
    return (access(c_to_char_only(path), F_OK) == 0);
  }
  pebl_panic("cannot check for a non-ascii path");
}
int64_t c_pathIsFile(wchar_t* path) {
  if(c_is_char_only(path)) {
    struct stat path_stat;
    if(stat(c_to_char_only(path), &path_stat) != 0) return 0;
    return S_ISREG(path_stat.st_mode);
  }
  pebl_panic("cannot check for a non-ascii path");
}
