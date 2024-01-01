#ifndef COMMON_STRING_H_
#define COMMON_STRING_H_

#include <wchar.h>

char* bsstrdup(const char* str);

void bsstrcpy(char* dst, const char* src);
void peblwstrcpy(wchar_t* dst, const wchar_t* src);

char* bsstrcat(const char* str1, const char* str2);

wchar_t* peblwstrcat(const wchar_t* str1, const wchar_t* str2);

long long wcs_to_int(wchar_t* wcs);

#endif
