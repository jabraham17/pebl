#include "common/bsstring.h"

#include <stdlib.h>
#include <string.h>

char* bsstrdup(const char* str) {
  int len = strlen(str) + 1;
  char* new_str = malloc(sizeof(*new_str) * len);
  memcpy(new_str, str, len);
  return new_str;
}

void bsstrcpy(char* dst, const char* src) {
  int len = strlen(src) + 1;
  memcpy(dst, src, len);
}

char* bsstrcat(const char* str1, const char* str2) {
  int len1 = strlen(str1);
  int len2 = strlen(str2);
  char* new_str = malloc(sizeof(*new_str) * (len1 + len2 + 1));
  memcpy(new_str, str1, len1);
  bsstrcpy(new_str + len1, str2);
  return new_str;
}
