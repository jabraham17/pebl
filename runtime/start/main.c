#include <stdint.h>
#include <locale.h>

extern int64_t _bs_main_entry(int8_t** args, int64_t nargs);
int main(int argc, char** argv) {
  setlocale(LC_CTYPE,"");
  return _bs_main_entry((int8_t**)argv, (int64_t)argc);
}
