#include <stdint.h>
#include <locale.h>
#include <wchar.h>

wchar_t* c_to_wchar(char* s);
void* c_allocate(int64_t n);

int64_t _bs_main_entry(wchar_t** args, int64_t nargs);

int main(int argc, char** argv) {
  setlocale(LC_CTYPE,"");
  // allocate args
  int64_t nargs = (int64_t)argc;
  wchar_t** args = c_allocate(sizeof(*args)*nargs);
  for(int i = 0; i < argc; i++) {
    args[i] = c_to_wchar(argv[i]);
  }

  return _bs_main_entry(args, nargs);
}
