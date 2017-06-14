#!/bin/bash

export CPPFLAGS="-g3 -O0 -ggdb"
export CFLAGS="-g3 -O0 -ggdb"
export R_KEEP_PKG_SOURCE=yes
export CXX="g++ -std=c++14"

export R_COMPILE_PKGS=0
export R_DISABLE_BYTECODE=1
export R_ENABLE_JIT=0

./configure --with-blas --with-lapack --without-ICU --without-x \
            --without-tcltk --without-aqua --without-recommended-packages \
            --without-internal-tzcode --with-included-gettext --enable-rdt &&
make clean &&
make -j8 &&

cd rdt-plugins/promises &&
#make clean &&
cmake . &&
make &&
cd ../.. 


