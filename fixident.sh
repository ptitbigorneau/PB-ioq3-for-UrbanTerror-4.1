#!/bin/bash

cd $(dirname "$0")

find code/ \( -name '*.h' -o -name '*.c' \) -a \( \! -name "qasm.h" \) -print -exec sed -r -i 's/\s+$//g' {} \;

find code/ \( -name '*.h' -o -name '*.c' \) -a \( \! -name "qasm.h" \) -print -exec indent {} \;

find code/ \( -name '*.h~' -o -name '*.c~' \) -print -delete

