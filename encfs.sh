#!/bin/bash
set -e
OUT=$PWD/out
cd encfs
make distclean || true
autoreconf
export lt_cv_deplibs_check_method='pass_all'
LDFLAGS=-L$OUT/lib CPPFLAGS=-I$OUT/include ./configure --prefix=$OUT --with-boost=$OUT --without-libiconv-prefix --without-libintl-prefix --host=i586-mingw32msvc
cd encfs
make encfs.exe
cp .libs/encfs.exe $OUT/bin
cd ../..
echo encfs ok

