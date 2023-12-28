#!/bin/bash -eu
# Supply build instructions
# Use the following environment variables to build the code
# $CXX:               c++ compiler
# $CC:                c compiler
# CFLAGS:             compiler flags for C files
# CXXFLAGS:           compiler flags for CPP files
# LIB_FUZZING_ENGINE: linker flag for fuzzing harnesses

mkdir build
cd build
cmake ../
make

# Copy all fuzzer executables to $OUT/
$CC $CFLAGS $LIB_FUZZING_ENGINE \
  $SRC/jsonsl/.clusterfuzzlite/parse_fuzzer.c \
  -o $OUT/parse_fuzzer \
  -I$SRC/jsonsl $PWD/libjsonsl.a
