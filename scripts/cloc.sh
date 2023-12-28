#/usr/bin/env bash


cloc --read-lang-def=doc/pebl_cloc.txt --include-lang=pebl,Python,C,'C/C++ Header' \
  src include test stdlib
