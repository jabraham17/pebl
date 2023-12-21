#/usr/bin/env bash
find src include -name '*.c' -o -name '*.h' | xargs clang-format -i
