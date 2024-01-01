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

void peblwstrcpy(wchar_t* dst, const wchar_t* src) {
  int len = wcslen(src) + 1;
  memcpy(dst, src, len * sizeof(*src));
}

char* bsstrcat(const char* str1, const char* str2) {
  int len1 = strlen(str1);
  int len2 = strlen(str2);
  char* new_str = malloc(sizeof(*new_str) * (len1 + len2 + 1));
  memcpy(new_str, str1, len1);
  bsstrcpy(new_str + len1, str2);
  return new_str;
}

wchar_t* peblwstrcat(const wchar_t* str1, const wchar_t* str2) {
  int len1 = wcslen(str1);
  int len2 = wcslen(str2);
  wchar_t* new_str = malloc(sizeof(*new_str) * (len1 + len2 + 1));

  memcpy(new_str, str1, len1 * sizeof(*str1));
  wcscpy(new_str + len1, str2);
  return new_str;
}

long long wcs_to_int(wchar_t* wcs) {
  wchar_t* end;
  return wcstoll(wcs, &end, 10);
}
