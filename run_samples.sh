#!/bin/bash

run() {
    echo "================================================="
    echo "=         Running $1"
    echo "================================================="
    make run-$1 ELF_ARGS="$2"
}

run add
run mul-div
run n\!
run simple-fuction
run qsort

run quick_sort '100000 1 1111'  # 1111 is random seed
run ackermann '3 8'
run matrix_mul '100 1 1111'  # 1111 is random seed
run malloc_free '50000 1111'  # 1111 is random seed
run dhrystone '200000'
